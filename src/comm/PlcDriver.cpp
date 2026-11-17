#include "comm/PlcDriver.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLibrary>
#include <QRegularExpression>
#include <QtEndian>

#include <array>
#include <bit>
#include <cstdint>
#include <map>

#ifdef Q_OS_WIN
#define HMI_S7_CALL __stdcall
#else
#define HMI_S7_CALL
#endif

namespace hmi
{

namespace
{
constexpr int kTimeoutMs = 200;

struct S7Address
{
    int db;
    int offset;
    int bit;
    int bytes;
};

Result<S7Address> s7Address(const Tag& tag)
{
    static const QRegularExpression pattern(
        QStringLiteral("^DB(\\d+)\\.DB([XBWD])(\\d+)(?:\\.([0-7]))?$"));
    const auto match = pattern.match(tag.address.toUpper());
    if (!match.hasMatch())
        return makeError(
            ErrorCode::InvalidArgument,
            QStringLiteral("Use an absolute address such as DB1.DBX0.0, DB1.DBW2 or DB1.DBD4"));
    bool dbOk = false;
    bool offsetOk = false;
    const int db = match.captured(1).toInt(&dbOk);
    const int offset = match.captured(3).toInt(&offsetOk);
    const QString kind = match.captured(2);
    const bool bit = kind == u"X";
    const int bytes = kind == u"D" ? 4 : kind == u"W" ? 2 : 1;
    if (!dbOk || !offsetOk || db < 1 || db > 65535 || offset > 65536 - bytes ||
        bit != !match.captured(4).isEmpty() || (tag.type == Tag::Type::Bool && !bit) ||
        (tag.type != Tag::Type::Bool && bit) || (tag.type == Tag::Type::Real && bytes != 4) ||
        tag.type == Tag::Type::String)
        return makeError(ErrorCode::TypeMismatch,
                         QStringLiteral("The S7 address does not match the HMI data type"));
    return S7Address{db, offset, match.captured(4).toInt(), bytes};
}

class NativeConnection
{
public:
    explicit NativeConnection(Connection settings)
        : config(std::move(settings))
    {
    }
    ~NativeConnection()
    {
        for (const auto& [name, handle] : handles)
        {
            Q_UNUSED(name)
            if (destroyTag)
                destroyTag(handle);
        }
        if (client && destroyS7)
            destroyS7(&client);
    }

    template <typename Function>
    bool resolve(Function& function, const char* name)
    {
        function = reinterpret_cast<Function>(library.resolve(name));
        return function != nullptr;
    }

    Result<void> load()
    {
        if (library.isLoaded())
            return loaded ? Result<void>()
                          : makeError(ErrorCode::Io,
                                      QStringLiteral("The driver library has an incompatible API"));
        const QString base =
            config.driver == u"s7" ? QStringLiteral("snap7") : QStringLiteral("plctag");
        library.setFileName(config.library.isEmpty()
                                ? QDir(QCoreApplication::applicationDirPath()).filePath(base)
                                : config.library);
        if (!library.load())
            return makeError(
                ErrorCode::Io,
                QStringLiteral("Cannot load %1. Set the native library path in Connections. %2")
                    .arg(base, library.errorString()));
        if (config.driver == u"s7")
        {
            loaded = resolve(createS7, "Cli_Create") && resolve(destroyS7, "Cli_Destroy") &&
                     resolve(connectS7, "Cli_ConnectTo") && resolve(readS7, "Cli_DBRead") &&
                     resolve(setParamS7, "Cli_SetParam");
            if (loaded)
            {
                client = createS7();
                if (!client)
                {
                    loaded = false;
                    return makeError(ErrorCode::Io,
                                     QStringLiteral("Snap7 could not create a client"));
                }
                int timeout = kTimeoutMs;
                if (setParamS7(client, 3, &timeout) || setParamS7(client, 4, &timeout) ||
                    setParamS7(client, 5, &timeout))
                {
                    loaded = false;
                    return makeError(ErrorCode::Io,
                                     QStringLiteral("Snap7 could not configure timeouts"));
                }
            }
        }
        else
        {
            loaded =
                resolve(createTag, "plc_tag_create") && resolve(destroyTag, "plc_tag_destroy") &&
                resolve(readTag, "plc_tag_read") && resolve(tagStatus, "plc_tag_status") &&
                resolve(getBit, "plc_tag_get_bit") && resolve(getInt16, "plc_tag_get_int16") &&
                resolve(getInt32, "plc_tag_get_int32") && resolve(getFloat, "plc_tag_get_float32");
        }
        return loaded ? Result<void>()
                      : makeError(ErrorCode::Io,
                                  QStringLiteral("The driver library has an incompatible API"));
    }

    Result<TagValue> read(const Tag& tag)
    {
        if (auto ready = load(); !ready)
            return tl::unexpected(ready.error());
        if (config.driver == u"s7")
        {
            const auto address = s7Address(tag);
            if (!address)
                return tl::unexpected(address.error());
            if (!connected)
            {
                const int result =
                    connectS7(client, config.host.toUtf8().constData(), config.rack, config.slot);
                if (result)
                    return makeError(ErrorCode::Io,
                                     QStringLiteral("S7 connection failed (%1)").arg(result));
                connected = true;
            }
            std::array<unsigned char, 4> data{};
            const int result =
                readS7(client, address->db, address->offset, address->bytes, data.data());
            if (result)
            {
                connected = false;
                return makeError(ErrorCode::Io, QStringLiteral("S7 read failed (%1)").arg(result));
            }
            if (tag.type == Tag::Type::Bool)
                return TagValue(bool(data[0] & (1U << address->bit)));
            if (tag.type == Tag::Type::Real)
                return TagValue(double(std::bit_cast<float>(qFromBigEndian<quint32>(data.data()))));
            if (address->bytes == 1)
                return TagValue(qint64(data[0]));
            if (address->bytes == 2)
                return TagValue(qint64(qFromBigEndian<qint16>(data.data())));
            return TagValue(qint64(qFromBigEndian<qint32>(data.data())));
        }

        auto found = handles.find(tag.name);
        if (found == handles.end())
        {
            auto text = PlcDriver::attributes(config, tag);
            if (!text)
                return tl::unexpected(text.error());
            const std::int32_t handle = createTag(text->toUtf8().constData(), kTimeoutMs);
            if (handle <= 0)
                return makeError(
                    ErrorCode::Io,
                    QStringLiteral("libplctag could not open the address (%1)").arg(handle));
            found = handles.emplace(tag.name, handle).first;
        }
        const auto handle = found->second;
        if (const int result = readTag(handle, kTimeoutMs); result != 0)
            return makeError(ErrorCode::Io,
                             QStringLiteral("libplctag read failed (%1)").arg(result));
        TagValue value;
        if (tag.type == Tag::Type::Bool)
            value = bool(getBit(handle, 0));
        else if (tag.type == Tag::Type::Real)
            value = double(getFloat(handle, 0));
        else if (config.driver == u"modbus-tcp")
            value = qint64(getInt16(handle, 0));
        else
            value = qint64(getInt32(handle, 0));
        if (const int result = tagStatus(handle); result != 0)
            return makeError(ErrorCode::Io,
                             QStringLiteral("libplctag conversion failed (%1)").arg(result));
        return value;
    }

    Connection config;
    QLibrary library;
    bool loaded = false;
    bool connected = false;
    std::uintptr_t client = 0;
    std::map<QString, std::int32_t> handles;
    std::uintptr_t(HMI_S7_CALL* createS7)() = nullptr;
    void(HMI_S7_CALL* destroyS7)(std::uintptr_t*) = nullptr;
    int(HMI_S7_CALL* connectS7)(std::uintptr_t, const char*, int, int) = nullptr;
    int(HMI_S7_CALL* readS7)(std::uintptr_t, int, int, int, void*) = nullptr;
    int(HMI_S7_CALL* setParamS7)(std::uintptr_t, int, void*) = nullptr;
    std::int32_t (*createTag)(const char*, int) = nullptr;
    int (*destroyTag)(std::int32_t) = nullptr;
    int (*readTag)(std::int32_t, int) = nullptr;
    int (*tagStatus)(std::int32_t) = nullptr;
    int (*getBit)(std::int32_t, int) = nullptr;
    std::int16_t (*getInt16)(std::int32_t, int) = nullptr;
    std::int32_t (*getInt32)(std::int32_t, int) = nullptr;
    float (*getFloat)(std::int32_t, int) = nullptr;
};
} // namespace

class PlcDriver::State
{
public:
    struct Target
    {
        Tag tag;
        NativeConnection* connection = nullptr;
        double nextPoll = 0;
        QString error;
        TagValue value;
    };
    std::map<QString, std::unique_ptr<NativeConnection>> connections;
    std::map<QString, Target> targets;
    QString lastPolled;
};

PlcDriver::PlcDriver(std::vector<Connection> connections)
    : m_state(std::make_unique<State>())
    , m_connections(std::move(connections))
{
}
PlcDriver::~PlcDriver() = default;

Result<void> PlcDriver::validateAddress(const Connection& connection, const Tag& tag)
{
    if (auto valid = connection.validate(); !valid)
        return valid;
    if (connection.driver == u"s7")
    {
        const auto address = s7Address(tag);
        return address ? Result<void>() : tl::unexpected(address.error());
    }
    if (tag.type == Tag::Type::String)
        return makeError(
            ErrorCode::TypeMismatch,
            QStringLiteral("PLC monitoring currently supports Bool, Int and Real scalars"));
    static const QRegularExpression symbol(QStringLiteral("^[A-Za-z_][A-Za-z0-9_:.\\[\\]]*$"));
    static const QRegularExpression modbus(QStringLiteral("^(co|di|hr|ir)(\\d+)$"));
    if (connection.driver == u"ab-eip")
    {
        if (!symbol.match(tag.address).hasMatch())
            return makeError(
                ErrorCode::InvalidArgument,
                QStringLiteral("Enter a Logix symbol such as Motor.Speed or Program:Main.Counter"));
        return {};
    }
    const auto match = modbus.match(tag.address);
    bool ok = false;
    const int offset = match.captured(2).toInt(&ok);
    const bool coil = match.captured(1) == u"co" || match.captured(1) == u"di";
    const int registers = tag.type == Tag::Type::Real ? 2 : 1;
    if (!match.hasMatch() || !ok || offset > 65536 - registers ||
        coil != (tag.type == Tag::Type::Bool))
        return makeError(
            ErrorCode::TypeMismatch,
            QStringLiteral("Use zero-based co0/di0 for Bool, or hr0/ir0 for Int and Real"));
    return {};
}

Result<QString> PlcDriver::attributes(const Connection& connection, const Tag& tag)
{
    if (auto valid = validateAddress(connection, tag); !valid)
        return tl::unexpected(valid.error());
    if (connection.driver == u"s7")
        return makeError(ErrorCode::InvalidArgument, QStringLiteral("S7 uses the Snap7 adapter"));
    const bool modbus = connection.driver == u"modbus-tcp";
    const int size = tag.type == Tag::Type::Bool ? 1 : modbus ? 2 : 4;
    const int count = modbus && tag.type == Tag::Type::Real ? 2 : 1;
    return QStringLiteral("protocol=%1&gateway=%2&path=%3&name=%4&elem_size=%5&elem_count=%6%7")
        .arg(modbus ? QStringLiteral("modbus-tcp") : QStringLiteral("ab-eip"), connection.host,
             modbus ? QString::number(connection.unit) : connection.path, tag.address)
        .arg(size)
        .arg(count)
        .arg(modbus ? QString() : QStringLiteral("&plc=controllogix"));
}

Result<void> PlcDriver::start(std::span<const Tag> tags)
{
    stop();
    for (const Connection& connection : m_connections)
        m_state->connections.emplace(connection.id, std::make_unique<NativeConnection>(connection));
    for (const Tag& tag : tags)
    {
        State::Target target{
            .tag = tag, .connection = nullptr, .nextPoll = 0, .error = {}, .value = {}};
        if (tag.connection.isEmpty())
            target.value = tag.startValue();
        else if (const auto it = m_state->connections.find(tag.connection);
                 it != m_state->connections.end())
        {
            target.connection = it->second.get();
            if (auto valid = validateAddress(target.connection->config, tag); !valid)
                target.error = valid.error().message;
        }
        else
            target.error = QStringLiteral("The configured PLC connection is missing");
        m_state->targets.emplace(tag.name.toLower(), std::move(target));
    }
    return {};
}

void PlcDriver::stop()
{
    m_state = std::make_unique<State>();
}

Result<void> PlcDriver::write(const QString&, const TagValue&)
{
    return makeError(ErrorCode::InvalidArgument, QStringLiteral("PLC monitoring is read only"));
}

QString PlcDriver::problem(const QString& tag) const
{
    const auto found = m_state->targets.find(tag.toLower());
    return found == m_state->targets.end() ? QStringLiteral("Unknown tag") : found->second.error;
}

void PlcDriver::poll(double seconds, const Publish& publish)
{
    // Bound a worker turn to one slow tag acquisition so stop() can run
    // between chunks even when a project contains many unreachable tags.
    QElapsedTimer budget;
    budget.start();
    auto it = m_state->targets.upper_bound(m_state->lastPolled);
    for (std::size_t visited = 0; visited < m_state->targets.size(); ++visited)
    {
        if (it == m_state->targets.end())
            it = m_state->targets.begin();
        m_state->lastPolled = it->first;
        State::Target& target = (it++)->second;
        if (seconds < target.nextPoll)
            continue;
        target.nextPoll = seconds + double(target.tag.acquisitionMs) / 1000.0;
        if (target.tag.connection.isEmpty())
            publish(target.tag.name, target.value);
        else if (target.connection)
        {
            if (auto valid = validateAddress(target.connection->config, target.tag); valid)
            {
                const auto value = target.connection->read(target.tag);
                target.error = value ? QString() : value.error().message;
                publish(target.tag.name, value ? *value : TagValue());
            }
        }
        if (budget.elapsed() >= kTimeoutMs)
            break;
    }
}

} // namespace hmi
