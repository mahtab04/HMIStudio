#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <qqmlintegration.h>

namespace hmi
{

class ToolContext;

// Connection setup shared by the tag table and its source picker. Every
// change is one undo operation and references use stable connection ids.
class TagConnections : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TagConnections is provided by TagModel")
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY draftChanged)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY draftChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QStringList driverKeys READ driverKeys CONSTANT)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QStringList names READ names NOTIFY changed)
    Q_PROPERTY(QStringList driverNames READ driverNames CONSTANT)

public:
    explicit TagConnections(ToolContext& context, QObject* parent = nullptr);
    [[nodiscard]] QVariantMap draft() const { return m_draft; }
    [[nodiscard]] int selectedRow() const { return m_selectedRow; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] QStringList driverKeys() const;
    Q_INVOKABLE void edit(int row);
    Q_INVOKABLE void setDraftField(const QString& field, const QVariant& value);
    Q_INVOKABLE void saveDraft();
    Q_INVOKABLE void setLibraryFile(const QUrl& file);
    [[nodiscard]] QVariantList entries() const;
    [[nodiscard]] QStringList names() const;
    [[nodiscard]] QStringList driverNames() const;
    Q_INVOKABLE bool save(int row, const QVariantMap& settings);
    Q_INVOKABLE bool remove(int row);
    [[nodiscard]] QString name(const QString& id) const;
    [[nodiscard]] QString idAt(int choice) const;
    [[nodiscard]] int choice(const QString& id) const;

Q_SIGNALS:
    void changed();
    void draftChanged();
    void messageChanged();
    void saved();
    void problem(const QString& message);

private:
    ToolContext& m_context;
    QVariantMap m_draft;
    int m_selectedRow = -1;
    QString m_message;
};

} // namespace hmi
