#include "editor/models/TagConnections.h"

#include "comm/PlcDriver.h"
#include "core/model/Project.h"
#include "editor/commands/TagCommands.h"
#include "editor/tools/ToolContext.h"

#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

namespace hmi
{

TagConnections::TagConnections(ToolContext& context, QObject* parent)
    : QObject(parent)
    , m_context(context)
{
    connect(&context.project().notifier(), &DocumentNotifier::connectionsChanged, this,
            &TagConnections::changed);
    connect(&context.project().notifier(), &DocumentNotifier::projectReset, this,
            &TagConnections::changed);
    connect(this, &TagConnections::problem, this,
            [this](const QString& message)
            {
                m_message = message;
                Q_EMIT messageChanged();
            });
    connect(&context.project().notifier(), &DocumentNotifier::projectReset, this,
            [this] { edit(-1); });
    edit(-1);
}

TagConnections::~TagConnections()
{
    if (m_test)
        m_test->abort();
}

namespace
{
// The PLC types of the dialog: a driver and its usual slot.
struct PlcType
{
    const char* driver;
    int slot;
};
constexpr PlcType kPlcTypes[] = {{"s7", 1}, {"s7", 2}, {"modbus-tcp", 1}, {"ab-eip", 1}};
} // namespace

QStringList TagConnections::plcTypeNames() const
{
    return {tr("Siemens S7-1200 / S7-1500"), tr("Siemens S7-300 / S7-400"), tr("Modbus TCP"),
            tr("Allen-Bradley (EtherNet/IP)")};
}

int TagConnections::plcType() const
{
    const QString driver = m_draft.value(QStringLiteral("driver")).toString();
    if (driver == u"modbus-tcp")
        return 2;
    if (driver == u"ab-eip")
        return 3;
    return m_draft.value(QStringLiteral("slot"), 1).toInt() == 2 ? 1 : 0;
}

void TagConnections::setPlcType(int type)
{
    if (type < 0 || type >= int(std::size(kPlcTypes)))
        return;
    m_draft.insert(QStringLiteral("driver"), QString::fromLatin1(kPlcTypes[type].driver));
    m_draft.insert(QStringLiteral("slot"), kPlcTypes[type].slot);
    m_draft.insert(QStringLiteral("rack"), 0);
    m_testResult.clear();
    Q_EMIT testChanged();
    Q_EMIT draftChanged();
}

QString TagConnections::address() const
{
    const QString host = m_draft.value(QStringLiteral("host")).toString();
    const int port = m_draft.value(QStringLiteral("port")).toInt();
    return port > 0 ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
}

void TagConnections::setAddress(const QString& text)
{
    // host[:port]; a port that is not a number stays in the host, where
    // saving reports it.
    QString host = text.trimmed();
    int port = 0;
    if (const qsizetype colon = host.lastIndexOf(u':'); colon > 0)
    {
        bool ok = false;
        const int value = host.mid(colon + 1).toInt(&ok);
        if (ok)
        {
            port = value;
            host.truncate(colon);
        }
    }
    m_draft.insert(QStringLiteral("host"), host);
    m_draft.insert(QStringLiteral("port"), port);
    m_testResult.clear();
    Q_EMIT testChanged();
    Q_EMIT draftChanged();
}

QString TagConnections::addressHint() const
{
    return PlcDriver::addressExamples(m_draft.value(QStringLiteral("driver")).toString());
}

bool TagConnections::testing() const
{
    return m_test != nullptr;
}

void TagConnections::testConnection()
{
    if (m_test)
        m_test->abort();
    const QString host = m_draft.value(QStringLiteral("host")).toString().trimmed();
    Connection probe;
    probe.driver = m_draft.value(QStringLiteral("driver")).toString();
    probe.port = m_draft.value(QStringLiteral("port")).toInt();
    const int port = probe.effectivePort();
    if (host.isEmpty())
    {
        m_testPassed = false;
        m_testResult = tr("Enter the PLC's IP address first.");
        Q_EMIT testChanged();
        return;
    }
    auto* socket = new QTcpSocket(this);
    m_test = socket;
    m_testPassed = false;
    m_testResult = tr("Connecting to %1:%2 …").arg(host).arg(port);
    Q_EMIT testChanged();
    const auto finish = [this, socket](bool passed, const QString& result)
    {
        if (m_test != socket)
            return; // a newer test, or the result is in already
        m_test = nullptr;
        m_testPassed = passed;
        m_testResult = result;
        socket->abort();
        socket->deleteLater();
        Q_EMIT testChanged();
    };
    connect(socket, &QTcpSocket::connected, this,
            [finish, host, port]
            {
                finish(true, tr("The PLC answers at %1:%2. Values are read when the runtime "
                                "starts.")
                                 .arg(host)
                                 .arg(port));
            });
    connect(socket, &QTcpSocket::errorOccurred, this,
            [finish, socket, host, port]
            {
                finish(false,
                       tr("No answer at %1:%2: %3").arg(host).arg(port).arg(socket->errorString()));
            });
    QTimer::singleShot(kTestTimeoutMs, socket,
                       [finish, host, port]
                       {
                           finish(false, tr("No answer at %1:%2 within %3 s. Check the IP "
                                            "address, the cable and the PLC's port.")
                                             .arg(host)
                                             .arg(port)
                                             .arg(kTestTimeoutMs / 1000));
                       });
    socket->connectToHost(host, quint16(port));
}

QStringList TagConnections::driverKeys() const
{
    return {QStringLiteral("s7"), QStringLiteral("ab-eip"), QStringLiteral("modbus-tcp")};
}

void TagConnections::edit(int row)
{
    const auto connections = m_context.project().connections();
    m_selectedRow = row >= 0 && std::size_t(row) < connections.size() ? row : -1;
    m_draft = m_selectedRow < 0 ? Connection{}.toJson().toVariantMap()
                                : connections[std::size_t(row)].toJson().toVariantMap();
    m_message.clear();
    m_testResult.clear();
    m_testPassed = false;
    Q_EMIT draftChanged();
    Q_EMIT messageChanged();
    Q_EMIT testChanged();
}

void TagConnections::setDraftField(const QString& field, const QVariant& value)
{
    m_draft.insert(field, value);
    Q_EMIT draftChanged();
}

void TagConnections::setLibraryFile(const QUrl& file)
{
    if (!file.isLocalFile())
        return;
    setDraftField(QStringLiteral("library"), file.toLocalFile());
    Q_EMIT libraryChosen(file.toLocalFile());
}

void TagConnections::saveDraft()
{
    if (save(m_selectedRow, m_draft))
    {
        edit(-1);
        Q_EMIT saved();
    }
}

QVariantList TagConnections::entries() const
{
    QVariantList result;
    for (const Connection& connection : m_context.project().connections())
        result.append(connection.toJson().toVariantMap());
    return result;
}

QStringList TagConnections::names() const
{
    QStringList result{tr("Internal")};
    for (const Connection& connection : m_context.project().connections())
        result.append(connection.name);
    return result;
}

QStringList TagConnections::driverNames() const
{
    return {tr("Siemens S7"), tr("Allen-Bradley (EtherNet/IP)"), tr("Modbus TCP")};
}

QString TagConnections::name(const QString& id) const
{
    if (id.isEmpty())
        return tr("Internal");
    const Connection* connection = m_context.project().connection(id);
    return connection ? connection->name : tr("Missing connection (%1)").arg(id);
}

QString TagConnections::idAt(int choice) const
{
    const auto connections = m_context.project().connections();
    return choice > 0 && std::size_t(choice) <= connections.size()
               ? connections[std::size_t(choice - 1)].id
               : QString();
}

int TagConnections::choice(const QString& id) const
{
    if (id.isEmpty())
        return 0;
    const auto connections = m_context.project().connections();
    for (std::size_t i = 0; i < connections.size(); ++i)
        if (connections[i].id == id)
            return int(i) + 1;
    return -1;
}

bool TagConnections::save(int row, const QVariantMap& settings)
{
    const auto current = m_context.project().connections();
    if (row < -1 || (row >= 0 && std::size_t(row) >= current.size()))
        return false;
    if (row >= 0 && settings.contains(QStringLiteral("id")) &&
        settings.value(QStringLiteral("id")).toString() != current[std::size_t(row)].id)
    {
        Q_EMIT problem(tr("The connection changed. Select it again before saving."));
        return false;
    }
    QJsonObject json = row < 0 ? Connection{}.toJson() : current[std::size_t(row)].toJson();
    for (auto it = settings.cbegin(); it != settings.cend(); ++it)
        json.insert(it.key(), QJsonValue::fromVariant(it.value()));
    json.insert(QStringLiteral("id"), row < 0 ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                              : current[std::size_t(row)].id);
    for (const char* field : {"name", "host", "path", "library"})
        json.insert(QLatin1StringView(field),
                    json.value(QLatin1StringView(field)).toString().trimmed());
    auto connection = Connection::fromJson(json);
    if (!connection)
    {
        Q_EMIT problem(connection.error().message);
        return false;
    }
    for (std::size_t i = 0; i < current.size(); ++i)
    {
        if (int(i) != row && current[i].name.toCaseFolded() == connection->name.toCaseFolded())
        {
            Q_EMIT problem(tr("A connection called %1 already exists.").arg(connection->name));
            return false;
        }
    }
    std::vector<Connection> after(current.begin(), current.end());
    if (row < 0)
        after.push_back(std::move(*connection));
    else
        after[std::size_t(row)] = std::move(*connection);
    m_context.undoStack().push(new ChangeConnectionsCommand(m_context.project(), std::move(after)));
    return true;
}

bool TagConnections::remove(int row)
{
    const auto current = m_context.project().connections();
    if (row < 0 || std::size_t(row) >= current.size())
        return false;
    for (const Tag& tag : m_context.project().tags().tags())
    {
        if (tag.connection == current[std::size_t(row)].id)
        {
            Q_EMIT problem(tr("%1 uses this connection. Change its source first.").arg(tag.name));
            return false;
        }
    }
    std::vector<Connection> after(current.begin(), current.end());
    after.erase(after.begin() + row);
    m_context.undoStack().push(new ChangeConnectionsCommand(m_context.project(), std::move(after)));
    edit(-1);
    return true;
}

} // namespace hmi
