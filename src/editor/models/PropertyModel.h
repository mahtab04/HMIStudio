#pragma once

#include "core/model/Ids.h"

#include <QAbstractListModel>
#include <QStringList>
#include <qqmlintegration.h>

#include <vector>

namespace hmi
{

struct PropertyDescriptor;
class HmiObject;
class ToolContext;

// The property panel's rows, built from the descriptors of the selected
// objects: a property shown for a multi-selection is one every selected
// object has, and `mixed` tells when their values differ. With nothing
// selected the rows are the current screen's own settings (name, size,
// background). Every edit pushes a command; edits between beginEdit() and
// endEdit() (a held stepper button) merge into one undo step.
//
// Values are exchanged with QML as QVariant: numbers, strings, colours,
// fonts, points, sizes and booleans; an enum as the index into `options`.
// QML picks a delegate by the `editor` role.
class PropertyModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("PropertyModel is provided by the application")
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QStringList fontFamilies READ fontFamilies CONSTANT)
    // Transform kinds for the binding editor, and their display names.
    Q_PROPERTY(QStringList transformKinds READ transformKinds CONSTANT)
    Q_PROPERTY(QStringList transformNames READ transformNames CONSTANT)

public:
    enum Role
    {
        KeyRole = Qt::UserRole + 1,
        NameRole,
        CategoryRole,
        EditorRole, // "number", "integer", "text", "color", "font", "enum", "bool", "point",
                    // "size", "points", "asset", "tag"
        ValueRole,
        MixedRole,
        ReadOnlyRole,
        MinimumRole,
        MaximumRole,
        OptionsRole,
        TextRole,       // the value as text for display and text fields
        ComponentsRole, // [x, y] of a point or [width, height] of a size
        BindableRole,   // the property can be driven by a tag
        BindingRole,    // "Tag · summary" of its binding, or empty
    };

    explicit PropertyModel(ToolContext& context, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString title() const;
    [[nodiscard]] QStringList fontFamilies() const;
    [[nodiscard]] QStringList transformKinds() const;
    [[nodiscard]] QStringList transformNames() const;

    // Sets the row's value on every selected object (or on the screen).
    // Returns false, and emits editRejected, when the value is not allowed.
    Q_INVOKABLE bool setValue(int row, const QVariant& value);
    // One coordinate of a point (x = 0, y = 1) or size (width = 0, height = 1).
    Q_INVOKABLE bool setComponent(int row, int component, const QVariant& value);
    // Adds `steps` (negative to subtract) to a number.
    Q_INVOKABLE bool stepValue(int row, int steps);
    Q_INVOKABLE bool setFontFamily(int row, const QString& family);
    Q_INVOKABLE bool setFontPixelSize(int row, int pixelSize);
    Q_INVOKABLE bool setFontBold(int row, bool bold);
    Q_INVOKABLE bool setFontItalic(int row, bool italic);

    // How importAsset treats SVG content the renderer cannot draw.
    enum ImportMode
    {
        AskAboutSvg, // emit svgImportQuestion and import nothing yet
        ImportAsIs,
        Rasterize, // store the SVG as a PNG of its natural size
    };
    Q_ENUM(ImportMode)

    // Imports an image or SVG file into the project's assets and makes it
    // the row's graphic. Returns false if nothing was imported.
    Q_INVOKABLE bool importAsset(int row, const QUrl& file,
                                 hmi::PropertyModel::ImportMode mode = AskAboutSvg);

    // The row's binding on the first selected object, for the binding
    // editor: {tag, kind, parameters, summary}; empty when there is none.
    Q_INVOKABLE QVariantMap bindingInfo(int row) const;
    // Binds the row's property on every selected object to `tag` through a
    // transform of `kind` with `parameters`. Rejected (editRejected) when the
    // property is not bindable or the transform's parameters are invalid.
    Q_INVOKABLE bool setBinding(int row, const QString& tag, const QString& kind,
                                const QVariantMap& parameters);
    Q_INVOKABLE void removeBinding(int row);
    // The link button of a row was pressed: emits bindingRequested so the
    // panel opens the binding editor for it.
    Q_INVOKABLE void requestBinding(int row);
    // The ranges of a "ranges" transform as editable text, one
    // "from..to = value" per line and "else = value" for the default, and
    // back. rangesParameters returns an empty map, and emits editRejected,
    // for a line it cannot read.
    Q_INVOKABLE QString rangesText(const QVariantMap& parameters) const;
    Q_INVOKABLE QVariantMap rangesParameters(const QString& text);

    Q_INVOKABLE void beginEdit();
    Q_INVOKABLE void endEdit();

    // Rebuilds the rows for the current selection and screen.
    void refresh();

Q_SIGNALS:
    void titleChanged();
    void bindingRequested(int row, const QString& name);
    void editRejected(const QString& message);
    // An SVG file has content that will not be drawn; QML asks whether to
    // import it as is or rasterize it, then calls importAsset again.
    void svgImportQuestion(int row, const QUrl& file, const QString& elements);

private:
    // Rows shown with nothing selected: the screen's settings, then the
    // project style.
    enum class ScreenField
    {
        Name,
        Width,
        Height,
        Background,
        StyleFace,
        StyleLine,
        StyleText,
        StyleAccent,
        StyleFont,
    };

    struct Row
    {
        const PropertyDescriptor* descriptor = nullptr; // object property
        ScreenField field = ScreenField::Name;          // screen row when descriptor is null
        // The "Process · Tag" row: picks the tag of the process property.
        bool tag = false;
    };

    [[nodiscard]] std::vector<HmiObject*> selectedObjects() const;
    [[nodiscard]] QVariant objectValue(const Row& row, bool* mixed) const;
    [[nodiscard]] QVariant screenValue(ScreenField field) const;
    [[nodiscard]] QVariant currentValue(int row) const;
    bool applyObjectValue(const Row& row, const QVariant& value);
    bool applyScreenValue(ScreenField field, const QVariant& value);
    bool applyStyleValue(ScreenField field, const QVariant& value);
    void onPropertyChanged(ObjectId object, PropertyId property);
    [[nodiscard]] QString bindingText(const Row& row) const;
    [[nodiscard]] QVariant tagData(const Row& row, int role) const;
    bool applyTag(const Row& row, const QString& tag);
    bool reject(const QString& message);
    [[nodiscard]] static bool isBlank(const QVariant& value);

    ToolContext& m_context;
    std::vector<Row> m_rows;
    int m_editSession = 0; // non-zero while an edit is held
    int m_nextSession = 1;
};

} // namespace hmi
