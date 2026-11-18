#include "comm/PlcDriver.h"

#include "comm/PlcProtocols.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLibrary>
#include <QRegularExpression>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <map>

namespace hmi
{

namespace
{
constexpr int kTimeoutMs = 200; // libplctag calls

// An S7 absolute address: a data block (DB1.DBW2), inputs (I0.0, IW64),
// outputs (Q0.1, QB0) or markers (M0.0, MW10, MD20), with an optional %
// as TIA Portal writes them; E and A are the German I and Q.
Result<S7::Item> parseS7(const QString& text)
{
    static const QRegularExpression db(
        QStringLiteral("^%?DB(\\d+)\\.DB([XBWD])(\\d+)(?:\\.([0-7]))?$"));
    static const QRegularExpression area(
        QStringLiteral("^%?([IEQAM])([BWD]?)(\\d+)(?:\\.([0-7]))?$"));
    const QString address = text.trimmed().toUpper();
    S7::Item result;
    QString size;
    QString bit;
    bool ok = false;
    if (const auto m = db.match(address); m.hasMatch())
    {
        result.area = S7::DataBlock;
        result.db = m.captured(1).toInt(&ok);
        if (!ok || result.db < 1 || result.db > 65535)
            ok = false;
        size = m.captured(2) == u"X" ? QString() : m.captured(2);
        result.byte = m.captured(3).toInt();
        bit = m.captured(4);
        if ((m.captured(2) == u"X") == bit.isEmpty())
            ok = false;
    }
    else if (const auto a = area.match(address); a.hasMatch())
    {
        const QChar letter = a.captured(1).front();
        result.area = letter == u'M'                       ? S7::Markers
                      : (letter == u'I' || letter == u'E') ? S7::Inputs
                                                           : S7::Outputs;
        size = a.captured(2);
        result.byte = a.captured(3).toInt(&ok);
        bit = a.captured(4);
        if (size.isEmpty() == bit.isEmpty())
            ok = false; // M0.0 or MW0, not M0 or MW0.0
    }
    if (!ok)
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Use an S7 address such as DB1.DBX0.0, DB1.DBW2, "
                                        "DB1.DBD4, M0.0, MW10, I0.0 or Q0.1"));
    result.bytes = size == u"D" ? 4 : size == u"W" ? 2 : 1;
    result.bit = bit.isEmpty() ? -1 : bit.toInt();
    if (result.byte > 65536 - result.bytes)
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("The S7 address is out of range"));
    return result;
}

Result<S7::Item> s7Address(const Tag& tag)
{
    auto address = parseS7(tag.address);
    if (!address)
        return address;
    const bool bit = address->bit >= 0;
    if ((tag.type == Tag::Type::Bool) != bit ||
        (tag.type == Tag::Type::Real && address->bytes != 4) || tag.type == Tag::Type::String)
        return makeError(ErrorCode::TypeMismatch,
                         QStringLiteral("The address does not fit the data type: a Bool needs a "
                                        "bit (DB1.DBX0.0, M0.0), a Real a double word "
                                        "(DB1.DBD4, MD20)"));
    return address;
}

qint64 rounded(const TagValue& value, qint64 low, qint64 high)
{
    return std::clamp<qint64>(std::llround(TagValues::toNumber(value).value_or(0.0)), low, high);
}

// One PLC: S7 and Modbus TCP through the built-in clients, Allen-Bradley
// through the libplctag library.
class NativeConnection
{
public:
    NativeConnection(Connection settings, bool libraryTrusted)
        : config(std::move(settings))
        , trusted(libraryTrusted)
    {
        if (config.driver == u"s7")
            s7 = std::make_unique<S7Client>(config.host, config.effectivePort(), config.rack,
                                            config.slot);
        else if (config.driver == u"modbus-tcp")
            modbus =
                std::make_unique<ModbusClient>(config.host, config.effectivePort(), config.unit);
    }
    ~NativeConnection()
    {
        for (const auto& [name, handle] : handles)
        {
            Q_UNUSED(name)
            if (destroyTag)
                destroyTag(handle);
        }
    }
    NativeConnection(const NativeConnection&) = delete;
    NativeConnection& operator=(const NativeConnection&) = delete;
    NativeConnection(NativeConnection&&) = delete;
    NativeConnection& operator=(NativeConnection&&) = delete;

    template <typename Function>
    bool resolve(Function& function, const char* name)
    {
        function = reinterpret_cast<Function>(library.resolve(name));
        return function != nullptr;
    }

    // libplctag, for Allen-Bradley.
    Result<void> load()
    {
        if (library.isLoaded())
            return loaded ? Result<void>()
                          : makeError(ErrorCode::Io,
                                      QStringLiteral("The driver library has an incompatible API"));
        if (!config.library.isEmpty() && !trusted)
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("The project names the library %1. For your safety "
                                            "it is loaded only after you choose it with Browse "
                                            "in Connections on this computer.")
                                 .arg(QDir::toNativeSeparators(config.library)));
        library.setFileName(
            config.library.isEmpty()
                ? QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plctag"))
                : config.library);
        if (!library.load())
            return makeError(ErrorCode::Io,
                             QStringLiteral("Allen-Bradley PLCs need the libplctag library "
                                            "(plctag.dll) beside the application. %1")
                                 .arg(library.errorString()));
        loaded = resolve(createTag, "plc_tag_create") && resolve(destroyTag, "plc_tag_destroy") &&
                 resolve(readTag, "plc_tag_read") && resolve(writeTag, "plc_tag_write") &&
                 resolve(tagStatus, "plc_tag_status") && resolve(getBit, "plc_tag_get_bit") &&
                 resolve(getInt32, "plc_tag_get_int32") &&
                 resolve(getFloat, "plc_tag_get_float32") && resolve(setBit, "plc_tag_set_bit") &&
                 resolve(setInt32, "plc_tag_set_int32") && resolve(setFloat, "plc_tag_set_float32");
        return loaded ? Result<void>()
                      : makeError(ErrorCode::Io,
                                  QStringLiteral("The driver library has an incompatible API"));
    }

    Result<std::int32_t> handleFor(const Tag& tag)
    {
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
                    QStringLiteral("No connection to the PLC at %1, or no such tag (%2)")
                        .arg(config.host)
                        .arg(handle));
            found = handles.emplace(tag.name, handle).first;
        }
        return found->second;
    }

    Result<TagValue> read(const Tag& tag)
    {
        if (s7)
            return readS7(tag);
        if (modbus)
            return readModbus(tag);
        if (auto ready = load(); !ready)
            return tl::unexpected(ready.error());
        const auto handle = handleFor(tag);
        if (!handle)
            return tl::unexpected(handle.error());
        if (const int result = readTag(*handle, kTimeoutMs); result != 0)
            return makeError(ErrorCode::Io, QStringLiteral("Read failed (%1)").arg(result));
        TagValue value;
        if (tag.type == Tag::Type::Bool)
            value = bool(getBit(*handle, 0));
        else if (tag.type == Tag::Type::Real)
            value = double(getFloat(*handle, 0));
        else
            value = qint64(getInt32(*handle, 0));
        if (const int result = tagStatus(*handle); result != 0)
            return makeError(ErrorCode::Io, QStringLiteral("Read failed (%1)").arg(result));
        return value;
    }

    Result<void> write(const Tag& tag, const TagValue& value)
    {
        if (s7)
            return writeS7(tag, value);
        if (modbus)
            return writeModbus(tag, value);
        if (auto ready = load(); !ready)
            return ready;
        const auto handle = handleFor(tag);
        if (!handle)
            return tl::unexpected(handle.error());
        int set = 0;
        if (tag.type == Tag::Type::Bool)
            set = setBit(*handle, 0, TagValues::toBool(value) ? 1 : 0);
        else if (tag.type == Tag::Type::Real)
            set = setFloat(*handle, 0, float(TagValues::toNumber(value).value_or(0.0)));
        else
            set = setInt32(*handle, 0, std::int32_t(rounded(value, INT32_MIN, INT32_MAX)));
        if (set != 0)
            return makeError(ErrorCode::Io, QStringLiteral("Write failed (%1)").arg(set));
        if (const int result = writeTag(*handle, kTimeoutMs); result != 0)
            return makeError(ErrorCode::Io, QStringLiteral("Write failed (%1)").arg(result));
        return {};
    }

    Result<TagValue> readS7(const Tag& tag)
    {
        const auto item = s7Address(tag);
        if (!item)
            return tl::unexpected(item.error());
        auto data = s7->read(*item);
        if (!data)
            return tl::unexpected(data.error());
        if (data->size() < (item->bit >= 0 ? 1 : item->bytes))
            return makeError(ErrorCode::BadFormat, QStringLiteral("A short reply from the PLC"));
        const auto* bytes = reinterpret_cast<const uchar*>(data->constData());
        if (tag.type == Tag::Type::Bool)
            return TagValue(bool(bytes[0] & 1));
        if (tag.type == Tag::Type::Real)
            return TagValue(double(std::bit_cast<float>(qFromBigEndian<quint32>(bytes))));
        if (item->bytes == 1)
            return TagValue(qint64(bytes[0]));
        if (item->bytes == 2)
            return TagValue(qint64(qFromBigEndian<qint16>(bytes)));
        return TagValue(qint64(qFromBigEndian<qint32>(bytes)));
    }

    Result<void> writeS7(const Tag& tag, const TagValue& value)
    {
        const auto item = s7Address(tag);
        if (!item)
            return tl::unexpected(item.error());
        std::array<uchar, 4> data{};
        if (tag.type == Tag::Type::Bool)
            data[0] = TagValues::toBool(value) ? 1 : 0;
        else if (tag.type == Tag::Type::Real)
            qToBigEndian(std::bit_cast<quint32>(float(TagValues::toNumber(value).value_or(0.0))),
                         data.data());
        else if (item->bytes == 1)
            data[0] = uchar(rounded(value, 0, 255));
        else if (item->bytes == 2)
            qToBigEndian(qint16(rounded(value, -32768, 32767)), data.data());
        else
            qToBigEndian(qint32(rounded(value, INT32_MIN, INT32_MAX)), data.data());
        return s7->write(*item, QByteArray(reinterpret_cast<const char*>(data.data()),
                                           item->bit >= 0 ? 1 : item->bytes));
    }

    Result<TagValue> readModbus(const Tag& tag)
    {
        const auto address = Modbus::parseAddress(tag.address);
        if (!address)
            return tl::unexpected(address.error());
        const int count = tag.type == Tag::Type::Real ? 2 : 1;
        auto data = modbus->read(address->table, address->offset, count);
        if (!data)
            return tl::unexpected(data.error());
        const auto* bytes = reinterpret_cast<const uchar*>(data->constData());
        if (tag.type == Tag::Type::Bool)
            return TagValue(bytes[0] != 0);
        if (tag.type == Tag::Type::Real) // high word first
            return TagValue(double(std::bit_cast<float>(qFromBigEndian<quint32>(bytes))));
        return TagValue(qint64(qFromBigEndian<qint16>(bytes)));
    }

    Result<void> writeModbus(const Tag& tag, const TagValue& value)
    {
        const auto address = Modbus::parseAddress(tag.address);
        if (!address)
            return tl::unexpected(address.error());
        if (address->table == Modbus::DiscreteInputs || address->table == Modbus::InputRegisters)
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Inputs (di, ir) cannot be written; use co or hr"));
        if (address->table == Modbus::Coils)
            return modbus->writeCoil(address->offset, TagValues::toBool(value));
        std::array<uchar, 4> data{};
        if (tag.type == Tag::Type::Real)
        {
            qToBigEndian(std::bit_cast<quint32>(float(TagValues::toNumber(value).value_or(0.0))),
                         data.data());
            return modbus->writeRegisters(
                address->offset, QByteArray(reinterpret_cast<const char*>(data.data()), 4));
        }
        qToBigEndian(qint16(rounded(value, -32768, 32767)), data.data());
        return modbus->writeRegisters(address->offset,
                                      QByteArray(reinterpret_cast<const char*>(data.data()), 2));
    }

    Connection config;
    bool trusted = false;
    std::unique_ptr<S7Client> s7;
    std::unique_ptr<ModbusClient> modbus;
    QLibrary library;
    bool loaded = false;
    std::map<QString, std::int32_t> handles;
    std::int32_t (*createTag)(const char*, int) = nullptr;
    int (*destroyTag)(std::int32_t) = nullptr;
    int (*readTag)(std::int32_t, int) = nullptr;
    int (*writeTag)(std::int32_t, int) = nullptr;
    int (*tagStatus)(std::int32_t) = nullptr;
    int (*getBit)(std::int32_t, int) = nullptr;
    std::int32_t (*getInt32)(std::int32_t, int) = nullptr;
    float (*getFloat)(std::int32_t, int) = nullptr;
    int (*setBit)(std::int32_t, int, int) = nullptr;
    int (*setInt32)(std::int32_t, int, std::int32_t) = nullptr;
    int (*setFloat)(std::int32_t, int, float) = nullptr;
};
} // namespace

class PlcDriver::State
{
public:
    struct Target
    {
        Tag tag;
        NativeConnection* connection = nullptr;
        bool addressValid = false; // checked once, at start
        double nextPoll = 0;
        QString error;
        TagValue value;
    };
    std::map<QString, std::unique_ptr<NativeConnection>> connections;
    std::map<QString, Target> targets;
    QString lastPolled;
};

PlcDriver::PlcDriver(std::vector<Connection> connections, QStringList trustedLibraries)
    : m_state(std::make_unique<State>())
    , m_connections(std::move(connections))
    , m_trustedLibraries(std::move(trustedLibraries))
{
}

bool PlcDriver::isTrustedLibrary(const QString& path, const QStringList& trusted)
{
    if (path.isEmpty())
        return false;
    const auto normal = [](const QString& file)
    {
        return QDir::cleanPath(QFileInfo(file).absoluteFilePath());
    };
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity kCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity kCase = Qt::CaseSensitive;
#endif
    const QString wanted = normal(path);
    return std::ranges::any_of(trusted, [&](const QString& entry)
                               { return normal(entry).compare(wanted, kCase) == 0; });
}
PlcDriver::~PlcDriver() = default;

std::optional<Tag::Type> PlcDriver::typeForAddress(const QString& driver, const QString& address)
{
    if (driver == u"s7")
    {
        const auto parsed = parseS7(address);
        if (!parsed)
            return std::nullopt;
        if (parsed->bit >= 0)
            return Tag::Type::Bool;
        return parsed->bytes == 4 ? Tag::Type::Real : Tag::Type::Int;
    }
    if (driver == u"modbus-tcp")
    {
        static const QRegularExpression modbus(QStringLiteral("^(co|di|hr|ir)\\d+$"));
        const auto match = modbus.match(address.trimmed().toLower());
        if (!match.hasMatch())
            return std::nullopt;
        return match.captured(1) == u"co" || match.captured(1) == u"di" ? Tag::Type::Bool
                                                                        : Tag::Type::Int;
    }
    return std::nullopt;
}

QString PlcDriver::addressExamples(const QString& driver)
{
    if (driver == u"modbus-tcp")
        return QCoreApplication::translate(
            "hmi::PlcDriver", "Addresses: co0 (coil), di0 (input), hr0 (holding register), "
                              "ir0 (input register), zero based. A Real uses two registers.");
    if (driver == u"ab-eip")
        return QCoreApplication::translate(
            "hmi::PlcDriver",
            "Addresses: tag names such as Motor.Running, Motor.Speed or Program:Main.Counter.");
    return QCoreApplication::translate(
        "hmi::PlcDriver",
        "Addresses: DB1.DBX0.0 (Bool), DB1.DBW2 (Int), DB1.DBD4 (Real), M0.0, MW10, I0.0, "
        "Q0.1. S7-1200/1500: allow PUT/GET access and use non-optimized data blocks.");
}

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
        return makeError(ErrorCode::TypeMismatch, QStringLiteral("PLC tags are Bool, Int or Real"));
    static const QRegularExpression symbol(QStringLiteral("^[A-Za-z_][A-Za-z0-9_:.\\[\\]]*$"));
    if (connection.driver == u"ab-eip")
    {
        if (!symbol.match(tag.address).hasMatch())
            return makeError(
                ErrorCode::InvalidArgument,
                QStringLiteral("Enter a Logix tag such as Motor.Speed or Program:Main.Counter"));
        return {};
    }
    const auto address = Modbus::parseAddress(tag.address);
    const bool bit =
        address && (address->table == Modbus::Coils || address->table == Modbus::DiscreteInputs);
    const int registers = tag.type == Tag::Type::Real ? 2 : 1;
    if (!address || address->offset > 65536 - registers || bit != (tag.type == Tag::Type::Bool))
        return makeError(
            ErrorCode::TypeMismatch,
            QStringLiteral("Use zero-based co0/di0 for Bool, or hr0/ir0 for Int and Real"));
    return {};
}

Result<QString> PlcDriver::attributes(const Connection& connection, const Tag& tag)
{
    if (auto valid = validateAddress(connection, tag); !valid)
        return tl::unexpected(valid.error());
    if (connection.driver != u"ab-eip")
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Only Allen-Bradley uses libplctag"));
    // The gateway names a port only when it is not the protocol's.
    const QString gateway = connection.port > 0
                                ? QStringLiteral("%1:%2").arg(connection.host).arg(connection.port)
                                : connection.host;
    return QStringLiteral("protocol=ab-eip&gateway=%1&path=%2&name=%3&elem_size=%4&elem_count=1"
                          "&plc=controllogix")
        .arg(gateway, connection.path, tag.address)
        .arg(tag.type == Tag::Type::Bool ? 1 : 4);
}

Result<void> PlcDriver::start(std::span<const Tag> tags)
{
    stop();
    for (const Connection& connection : m_connections)
        m_state->connections.emplace(
            connection.id,
            std::make_unique<NativeConnection>(
                connection, isTrustedLibrary(connection.library, m_trustedLibraries)));
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
            else
                target.addressValid = true;
        }
        else
            target.error = QStringLiteral("The tag's PLC connection is missing");
        m_state->targets.emplace(tag.name.toLower(), std::move(target));
    }
    return {};
}

void PlcDriver::stop()
{
    m_state = std::make_unique<State>();
}

Result<void> PlcDriver::write(const QString& name, const TagValue& value)
{
    const auto found = m_state->targets.find(name.toLower());
    if (found == m_state->targets.end())
        return makeError(ErrorCode::NotFound, QStringLiteral("Unknown tag"));
    State::Target& target = found->second;
    if (target.tag.connection.isEmpty())
    {
        target.value = value;
        target.nextPoll = 0;
        return {};
    }
    if (!target.connection || !target.addressValid)
        return makeError(ErrorCode::InvalidArgument, target.error);
    auto written = target.connection->write(target.tag, value);
    target.error = written ? QString() : written.error().message;
    // Read back at the next poll, so the HMI shows what the PLC holds.
    target.nextPoll = 0;
    return written;
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
        else if (target.connection && target.addressValid)
        {
            const auto value = target.connection->read(target.tag);
            target.error = value ? QString() : value.error().message;
            publish(target.tag.name, value ? *value : TagValue());
        }
        if (budget.elapsed() >= kTimeoutMs)
            break;
    }
}

} // namespace hmi
