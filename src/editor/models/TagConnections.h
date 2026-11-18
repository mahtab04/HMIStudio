#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <qqmlintegration.h>

class QTcpSocket;

namespace hmi
{

class ToolContext;

// Connection setup shared by the tag table and its source picker: a PLC is
// a name, a PLC type and an IP address (with an optional :port); the rest
// has defaults. Every change is one undo operation and references use
// stable connection ids.
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
    // The PLC types offered: S7-1200/1500, S7-300/400, Modbus TCP,
    // Allen-Bradley; and the draft's.
    Q_PROPERTY(QStringList plcTypeNames READ plcTypeNames CONSTANT)
    Q_PROPERTY(int plcType READ plcType NOTIFY draftChanged)
    // The draft's "IP" or "IP:port".
    Q_PROPERTY(QString address READ address NOTIFY draftChanged)
    Q_PROPERTY(QString addressHint READ addressHint NOTIFY draftChanged)
    // Test connection: running, and what it found.
    Q_PROPERTY(bool testing READ testing NOTIFY testChanged)
    Q_PROPERTY(QString testResult READ testResult NOTIFY testChanged)
    Q_PROPERTY(bool testPassed READ testPassed NOTIFY testChanged)

public:
    static constexpr int kTestTimeoutMs = 3000;

    explicit TagConnections(ToolContext& context, QObject* parent = nullptr);
    ~TagConnections() override;
    TagConnections(const TagConnections&) = delete;
    TagConnections& operator=(const TagConnections&) = delete;
    TagConnections(TagConnections&&) = delete;
    TagConnections& operator=(TagConnections&&) = delete;
    [[nodiscard]] QVariantMap draft() const { return m_draft; }
    [[nodiscard]] int selectedRow() const { return m_selectedRow; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] QStringList driverKeys() const;
    Q_INVOKABLE void edit(int row);
    Q_INVOKABLE void setDraftField(const QString& field, const QVariant& value);
    Q_INVOKABLE void saveDraft();
    Q_INVOKABLE void setLibraryFile(const QUrl& file);
    [[nodiscard]] QStringList plcTypeNames() const;
    [[nodiscard]] int plcType() const;
    Q_INVOKABLE void setPlcType(int type);
    [[nodiscard]] QString address() const;
    // "192.168.0.10" or "192.168.0.10:1102".
    Q_INVOKABLE void setAddress(const QString& text);
    [[nodiscard]] QString addressHint() const;
    // Whether the draft's PLC answers on its port (a TCP connection).
    Q_INVOKABLE void testConnection();
    [[nodiscard]] bool testing() const;
    [[nodiscard]] QString testResult() const { return m_testResult; }
    [[nodiscard]] bool testPassed() const noexcept { return m_testPassed; }
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
    void testChanged();
    void saved();
    // The user picked a native library with the file dialog: it may be
    // loaded on this computer from now on.
    void libraryChosen(const QString& path);
    void problem(const QString& message);

private:
    ToolContext& m_context;
    QVariantMap m_draft;
    int m_selectedRow = -1;
    QString m_message;
    QPointer<QTcpSocket> m_test;
    QString m_testResult;
    bool m_testPassed = false;
};

} // namespace hmi
