#pragma once

#include <QAbstractTableModel>
#include <QUrl>
#include <qqmlintegration.h>

#include <vector>

class QUndoStack;

namespace hmi
{

class Project;
class TagEngine;
class ToolContext;

// The Tags panel: one row per tag of the project, filtered by `filterText`
// (name, unit and comment), with a live value column while simulating.
// Every edit pushes a command; renaming a tag also renames it in every
// binding that uses it, in the same undo step.
class TagModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TagModel is provided by the application")
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QStringList columnTitles READ columnTitles CONSTANT)
    Q_PROPERTY(QStringList typeNames READ typeNames CONSTANT)
    Q_PROPERTY(QStringList simulationNames READ simulationNames CONSTANT)
    Q_PROPERTY(int tagCount READ tagCount NOTIFY tagCountChanged)

public:
    enum Column
    {
        NameColumn,
        TypeColumn,
        UnitColumn,
        MinimumColumn,
        MaximumColumn,
        SimulationColumn,
        PeriodColumn,
        CommentColumn,
        ValueColumn,
        ColumnCount,
    };
    Q_ENUM(Column)

    enum Role
    {
        // "text", "number", "type", "simulation", "value": which editor.
        EditorRole = Qt::UserRole + 1,
        // Index into typeNames / simulationNames for those columns.
        ChoiceRole,
    };

    TagModel(ToolContext& context, TagEngine& engine, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString filterText() const { return m_filter; }
    void setFilterText(const QString& text);
    [[nodiscard]] QStringList columnTitles() const;
    [[nodiscard]] QStringList typeNames() const;
    [[nodiscard]] QStringList simulationNames() const;
    [[nodiscard]] int tagCount() const;

    // Edits a cell; for QML, which has no setData for table cells.
    Q_INVOKABLE bool setCell(int row, int column, const QVariant& value);
    // Adds the next free Tag_N with the last tag's type; returns its row.
    Q_INVOKABLE int addTag();
    // Adds `count` tags of a type named name_1, name_2, … (skipping names
    // in use), in one undo step; returns the first one's row.
    Q_INVOKABLE int addTags(const QString& name, int typeIndex, int count);
    Q_INVOKABLE void removeTag(int row);
    // Places a tag is used, as texts ("02 Filling · T-101 · Level").
    Q_INVOKABLE QStringList whereUsed(int row) const;
    Q_INVOKABLE int usageCount(int row) const;
    // All tag names containing `text`, for the tag picker.
    Q_INVOKABLE QStringList namesMatching(const QString& text) const;

    Q_INVOKABLE bool exportCsv(const QUrl& file);
    // Adds the file's tags; tags with an existing name are updated. One undo
    // step. Returns the number of tags read, or -1 on error.
    Q_INVOKABLE int importCsv(const QUrl& file);

Q_SIGNALS:
    void filterTextChanged();
    void tagCountChanged();
    // Why an edit or an import was refused.
    void problem(const QString& message);

private:
    void rebuild();
    [[nodiscard]] const class Tag* tagAt(int row) const;
    bool renameTag(const QString& name, const QString& newName);

    ToolContext& m_context;
    TagEngine& m_engine;
    QString m_filter;
    std::vector<QString> m_rows; // tag names shown, in database order
};

} // namespace hmi
