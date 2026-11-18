#pragma once

#include "core/alarms/Alarm.h"

#include <QAbstractTableModel>
#include <QStringList>
#include <qqmlintegration.h>

#include <vector>

namespace hmi
{

class ToolContext;

// The HMI alarms editor, as in TIA Portal: three pages, Discrete alarms,
// Analog alarms and Alarm classes, each a table. Every edit is one undo step
// (ChangeAlarmsCommand); edits that would make the configuration invalid
// are refused with a reason (problem).
class AlarmModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AlarmModel is provided by the application")
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY pageChanged)
    Q_PROPERTY(QStringList pageNames READ pageNames CONSTANT)
    Q_PROPERTY(QStringList columnTitles READ columnTitles NOTIFY pageChanged)
    Q_PROPERTY(QStringList classNames READ classNames NOTIFY namesChanged)
    Q_PROPERTY(QStringList tagNames READ tagNames NOTIFY namesChanged)
    Q_PROPERTY(QStringList edgeNames READ edgeNames CONSTANT)
    Q_PROPERTY(QStringList modeNames READ modeNames CONSTANT)
    Q_PROPERTY(QStringList stateMachineNames READ stateMachineNames CONSTANT)
    Q_PROPERTY(QStringList yesNo READ yesNo CONSTANT)
    Q_PROPERTY(int alarmCount READ alarmCount NOTIFY namesChanged)

public:
    enum Page
    {
        DiscretePage,
        AnalogPage,
        ClassesPage,
    };
    Q_ENUM(Page)

    // Alarm pages.
    enum AlarmColumn
    {
        IdColumn,
        NameColumn,
        TextColumn,
        ClassColumn,
        TagColumn,
        BitColumn,   // discrete
        EdgeColumn,  // discrete
        ModeColumn,  // analog
        LimitColumn, // analog
        OriginColumn,
        InfoColumn,
        AlarmColumnCount,
    };
    Q_ENUM(AlarmColumn)

    // Alarm classes page: then background, text colour and flashing for
    // each of the four statuses (ClassColorColumn + 3 * state + 0/1/2).
    enum ClassPageColumn
    {
        ClassNameColumn,
        StateMachineColumn,
        PriorityColumn,
        ClassColorColumn,
        ClassColumnCount = ClassColorColumn + 3 * AlarmClass::ColorStateCount,
    };
    Q_ENUM(ClassPageColumn)

    enum Role
    {
        // "text", "number", "class", "tag", "edge", "mode", "stateMachine",
        // "color", "flag", "readonly": which editor.
        EditorRole = Qt::UserRole + 1,
        ChoiceRole, // index into the choices of that editor
        SwatchRole, // the colour of a colour cell
    };

    explicit AlarmModel(ToolContext& context, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int page() const noexcept { return int(m_page); }
    void setPage(int page);
    [[nodiscard]] QStringList pageNames() const;
    [[nodiscard]] QStringList columnTitles() const;
    [[nodiscard]] QStringList classNames() const;
    [[nodiscard]] QStringList tagNames() const;
    [[nodiscard]] QStringList edgeNames() const;
    [[nodiscard]] QStringList modeNames() const { return Alarms::modeNames(); }
    [[nodiscard]] QStringList stateMachineNames() const { return Alarms::stateMachineNames(); }
    [[nodiscard]] QStringList yesNo() const;
    [[nodiscard]] int alarmCount() const;

    // The width of a column on this page; 0 hides columns of other pages.
    Q_INVOKABLE double columnWidth(int column, double width, double minimum) const;
    Q_INVOKABLE bool setCell(int row, int column, const QVariant& value);
    // Adds an alarm (or a class on the classes page); returns its row.
    Q_INVOKABLE int addRow();
    // Removes an alarm, or a user-defined class no alarm uses.
    Q_INVOKABLE void removeRow(int row);

Q_SIGNALS:
    void pageChanged();
    void namesChanged();
    // Why an edit was refused.
    void problem(const QString& message);

private:
    void rebuild();
    // Indices into the configuration's alarms shown on an alarm page.
    [[nodiscard]] std::vector<std::size_t> alarmRows() const;
    [[nodiscard]] const AlarmConfiguration& configuration() const;
    bool change(AlarmConfiguration after, const QString& text);
    [[nodiscard]] QVariant alarmData(const Alarm& alarm, int column, int role) const;
    [[nodiscard]] QVariant classData(const AlarmClass& alarmClass, int column, int role) const;

    ToolContext& m_context;
    Page m_page = DiscretePage;
    std::vector<std::size_t> m_rows; // alarm indices on alarm pages
};

} // namespace hmi
