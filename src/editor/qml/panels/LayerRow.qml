import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One layer: drag handle, expand arrow for groups, icon, name, and the
// show/hide and lock toggles. Tap selects (Shift or multi-select mode adds);
// double-tap or long-press renames. Group children are shown indented and
// read-only.
Rectangle {
    id: row

    required property LayerModel layers
    required property EditorController editor
    required property Theme theme
    required property int index
    required property string name
    required property url iconSource
    required property int depth
    required property bool hasChildren
    required property bool expanded
    required property bool objectVisible
    required property bool locked
    required property bool selected
    required property bool editable

    property bool renaming: false

    implicitHeight: row.theme.controlHeight
    color: row.selected ? row.theme.selection : "transparent"
    radius: row.theme.radius

    // Shift adds to the selection; so does the touch multi-select mode.
    TapHandler {
        acceptedModifiers: Qt.ShiftModifier
        onTapped: row.layers.select(row.index, true)
    }
    TapHandler {
        acceptedModifiers: Qt.NoModifier
        onTapped: row.layers.select(row.index, row.editor.multiSelect)
        onDoubleTapped: row.renaming = row.editable
        onLongPressed: row.renaming = row.editable
    }

    transform: Translate {
        y: drag.active ? drag.activeTranslation.y : 0
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: row.theme.spacing / 2 + row.depth * row.theme.spacing * 2
        spacing: row.theme.spacing / 2

        // Drag handle: only here, so dragging elsewhere still scrolls the
        // list. The row follows the pointer; on release the model moves the
        // layer by the number of rows passed.
        IconButton {
            theme: row.theme
            label: qsTr("Drag to reorder")
            icon.source: "qrc:/icons/drag.svg"
            icon.color: row.theme.textMuted
            opacity: row.editable ? 1 : 0
            enabled: row.editable

            DragHandler {
                id: drag

                target: null
                enabled: row.editable
                xAxis.enabled: false
                onActiveChanged: if (!active) row.layers.moveBy(row.index, Math.round(drag.activeTranslation.y / row.height))
            }
        }
        IconButton {
            theme: row.theme
            label: row.expanded ? qsTr("Collapse") : qsTr("Expand")
            icon.source: row.expanded ? "qrc:/icons/arrow-down.svg" : "qrc:/icons/arrow-right.svg"
            opacity: row.hasChildren ? 1 : 0
            enabled: row.hasChildren
            onClicked: row.layers.toggleExpanded(row.index)
        }
        Image {
            source: row.iconSource
            sourceSize.width: row.theme.iconSize
            sourceSize.height: row.theme.iconSize
            opacity: row.objectVisible ? 1 : 0.4
            Accessible.ignored: true
        }

        Label {
            Layout.fillWidth: true
            visible: !row.renaming
            text: row.name
            elide: Text.ElideRight
            color: row.selected ? row.theme.accentText : row.theme.text
            opacity: row.objectVisible ? 1 : 0.5
        }
        TextField {
            Layout.fillWidth: true
            visible: row.renaming
            text: row.name
            implicitHeight: row.theme.controlHeight
            Accessible.name: qsTr("Layer name")
            onVisibleChanged: if (visible) forceActiveFocus()
            onEditingFinished: {
                row.layers.rename(row.index, text)
                row.renaming = false
            }
        }

        IconButton {
            theme: row.theme
            label: row.objectVisible ? qsTr("Hide") : qsTr("Show")
            icon.source: row.objectVisible ? "qrc:/icons/eye.svg" : "qrc:/icons/eye-off.svg"
            enabled: row.editable
            onClicked: row.layers.setObjectVisible(row.index, !row.objectVisible)
        }
        IconButton {
            theme: row.theme
            label: row.locked ? qsTr("Unlock") : qsTr("Lock")
            icon.source: row.locked ? "qrc:/icons/lock.svg" : "qrc:/icons/unlock.svg"
            enabled: row.editable
            onClicked: row.layers.setLocked(row.index, !row.locked)
        }
    }
}
