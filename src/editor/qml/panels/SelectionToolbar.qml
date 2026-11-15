import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Floating toolbar above the selection for touch use: delete, duplicate,
// group, z-order, nudge and a "more" button that opens the context menu.
// The parent places it from the canvas's selection bounds.
Pane {
    id: toolbar

    required property EditorController editor
    required property Theme theme
    required property Menu menu

    padding: toolbar.theme.spacing / 2

    background: Rectangle {
        color: toolbar.theme.panelRaised
        border.color: toolbar.theme.border
        radius: toolbar.theme.radius
    }

    RowLayout {
        spacing: 0

        IconButton {
            theme: toolbar.theme
            label: qsTr("Delete")
            icon.source: "qrc:/icons/delete.svg"
            onClicked: toolbar.editor.deleteSelection()
        }
        IconButton {
            theme: toolbar.theme
            label: qsTr("Duplicate")
            icon.source: "qrc:/icons/duplicate.svg"
            onClicked: toolbar.editor.duplicate()
        }
        IconButton {
            theme: toolbar.theme
            label: qsTr("Group")
            icon.source: "qrc:/icons/group.svg"
            visible: !toolbar.editor.canUngroup
            enabled: toolbar.editor.canGroup
            onClicked: toolbar.editor.group()
        }
        IconButton {
            theme: toolbar.theme
            label: qsTr("Ungroup")
            icon.source: "qrc:/icons/ungroup.svg"
            visible: toolbar.editor.canUngroup
            onClicked: toolbar.editor.ungroup()
        }
        IconButton {
            theme: toolbar.theme
            label: qsTr("Bring forward")
            icon.source: "qrc:/icons/bring-forward.svg"
            onClicked: toolbar.editor.reorder(EditorController.BringForward)
        }
        IconButton {
            theme: toolbar.theme
            label: qsTr("Send backward")
            icon.source: "qrc:/icons/send-backward.svg"
            onClicked: toolbar.editor.reorder(EditorController.SendBackward)
        }

        ToolSeparator {}

        IconButton {
            theme: toolbar.theme
            label: qsTr("Nudge left")
            icon.source: "qrc:/icons/arrow-left.svg"
            autoRepeat: true
            onClicked: toolbar.editor.nudge(-1, 0)
        }
        IconButton {
            theme: toolbar.theme
            label: qsTr("Nudge up")
            icon.source: "qrc:/icons/arrow-up.svg"
            autoRepeat: true
            onClicked: toolbar.editor.nudge(0, -1)
        }
        IconButton {
            theme: toolbar.theme
            label: qsTr("Nudge down")
            icon.source: "qrc:/icons/arrow-down.svg"
            autoRepeat: true
            onClicked: toolbar.editor.nudge(0, 1)
        }
        IconButton {
            theme: toolbar.theme
            label: qsTr("Nudge right")
            icon.source: "qrc:/icons/arrow-right.svg"
            autoRepeat: true
            onClicked: toolbar.editor.nudge(1, 0)
        }

        ToolSeparator {}

        IconButton {
            id: moreButton

            theme: toolbar.theme
            label: qsTr("More actions")
            icon.source: "qrc:/icons/more.svg"
            onClicked: toolbar.menu.popup(moreButton, 0, moreButton.height)
        }
    }
}
