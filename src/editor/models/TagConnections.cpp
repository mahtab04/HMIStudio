#include "editor/models/TagConnections.h"

#include "core/model/Project.h"
#include "editor/commands/TagCommands.h"
#include "editor/tools/ToolContext.h"

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
    Q_EMIT draftChanged();
    Q_EMIT messageChanged();
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
    return {tr("Siemens S7 · Snap7"), tr("Allen-Bradley Logix · libplctag"),
            tr("Modbus TCP · libplctag")};
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
