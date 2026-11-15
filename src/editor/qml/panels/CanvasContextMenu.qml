import QtQuick
import QtQuick.Controls

// Editing actions for the selection. Opened by right-click on the canvas and
// by the "more" button of the touch selection toolbar.
Menu {
    id: contextMenu

    required property EditorController editor

    MenuItem {
        text: qsTr("Cut")
        enabled: contextMenu.editor.hasSelection
        onTriggered: contextMenu.editor.cut()
    }
    MenuItem {
        text: qsTr("Copy")
        enabled: contextMenu.editor.hasSelection
        onTriggered: contextMenu.editor.copy()
    }
    MenuItem {
        text: qsTr("Paste")
        enabled: contextMenu.editor.canPaste
        onTriggered: contextMenu.editor.paste()
    }
    MenuItem {
        text: qsTr("Duplicate")
        enabled: contextMenu.editor.hasSelection
        onTriggered: contextMenu.editor.duplicate()
    }
    MenuItem {
        text: qsTr("Delete")
        enabled: contextMenu.editor.hasSelection
        onTriggered: contextMenu.editor.deleteSelection()
    }

    MenuSeparator {}

    MenuItem {
        text: qsTr("Group")
        enabled: contextMenu.editor.canGroup
        onTriggered: contextMenu.editor.group()
    }
    MenuItem {
        text: qsTr("Ungroup")
        enabled: contextMenu.editor.canUngroup
        onTriggered: contextMenu.editor.ungroup()
    }

    Menu {
        title: qsTr("Order")
        enabled: contextMenu.editor.hasSelection

        MenuItem {
            text: qsTr("Bring to front")
            onTriggered: contextMenu.editor.reorder(EditorController.BringToFront)
        }
        MenuItem {
            text: qsTr("Bring forward")
            onTriggered: contextMenu.editor.reorder(EditorController.BringForward)
        }
        MenuItem {
            text: qsTr("Send backward")
            onTriggered: contextMenu.editor.reorder(EditorController.SendBackward)
        }
        MenuItem {
            text: qsTr("Send to back")
            onTriggered: contextMenu.editor.reorder(EditorController.SendToBack)
        }
    }

    Menu {
        title: qsTr("Align")
        enabled: contextMenu.editor.canAlign

        MenuItem {
            text: qsTr("Left")
            onTriggered: contextMenu.editor.align(EditorController.AlignLeft)
        }
        MenuItem {
            text: qsTr("Centre", "horizontal alignment")
            onTriggered: contextMenu.editor.align(EditorController.AlignHorizontalCentre)
        }
        MenuItem {
            text: qsTr("Right")
            onTriggered: contextMenu.editor.align(EditorController.AlignRight)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Top")
            onTriggered: contextMenu.editor.align(EditorController.AlignTop)
        }
        MenuItem {
            text: qsTr("Middle", "vertical alignment")
            onTriggered: contextMenu.editor.align(EditorController.AlignVerticalCentre)
        }
        MenuItem {
            text: qsTr("Bottom")
            onTriggered: contextMenu.editor.align(EditorController.AlignBottom)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Distribute horizontally")
            enabled: contextMenu.editor.canDistribute
            onTriggered: contextMenu.editor.distribute(Qt.Horizontal)
        }
        MenuItem {
            text: qsTr("Distribute vertically")
            enabled: contextMenu.editor.canDistribute
            onTriggered: contextMenu.editor.distribute(Qt.Vertical)
        }
    }

    Menu {
        title: qsTr("Same size")
        enabled: contextMenu.editor.canMatchSize

        MenuItem {
            text: qsTr("Width")
            onTriggered: contextMenu.editor.matchSize(EditorController.SameWidth)
        }
        MenuItem {
            text: qsTr("Height")
            onTriggered: contextMenu.editor.matchSize(EditorController.SameHeight)
        }
        MenuItem {
            text: qsTr("Width and height")
            onTriggered: contextMenu.editor.matchSize(EditorController.SameSize)
        }
    }

    MenuSeparator {}

    MenuItem {
        text: qsTr("Select all")
        onTriggered: contextMenu.editor.selectAll()
    }
}
