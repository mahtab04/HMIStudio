import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Application bar: project title, tools, undo and redo, zoom, input density.
ToolBar {
    id: bar

    required property EditorController editor
    required property Theme theme

    leftPadding: bar.theme.padding
    rightPadding: bar.theme.padding

    RowLayout {
        anchors.fill: parent
        spacing: bar.theme.spacing

        ColumnLayout {
            spacing: 0
            Label {
                text: bar.editor.projectTitle
                font: bar.theme.fontHeading
                color: bar.theme.text
            }
            Label {
                text: qsTr("HMI Studio")
                font: bar.theme.fontSmall
                color: bar.theme.textMuted
            }
        }

        Item {
            Layout.fillWidth: true
        }

        IconButton {
            theme: bar.theme
            label: qsTr("Select (V)")
            icon.source: "qrc:/icons/tool-select.svg"
            checked: bar.editor.selectToolActive
            onClicked: bar.editor.selectTool()
        }
        IconButton {
            theme: bar.theme
            label: qsTr("Pan (H)")
            icon.source: "qrc:/icons/tool-pan.svg"
            checked: bar.editor.panToolActive
            onClicked: bar.editor.panTool()
        }
        ToolButton {
            text: qsTr("Multi-select")
            checked: bar.editor.multiSelect
            implicitHeight: bar.theme.controlHeight
            focusPolicy: Qt.NoFocus
            onClicked: bar.editor.toggleMultiSelect()
        }

        ToolSeparator {}

        IconButton {
            theme: bar.theme
            label: bar.editor.undoText
            icon.source: "qrc:/icons/undo.svg"
            enabled: bar.editor.canUndo
            onClicked: bar.editor.undo()
        }
        IconButton {
            theme: bar.theme
            label: bar.editor.redoText
            icon.source: "qrc:/icons/redo.svg"
            enabled: bar.editor.canRedo
            onClicked: bar.editor.redo()
        }

        ToolSeparator {}

        ToolButton {
            text: qsTr("−")
            Accessible.name: qsTr("Zoom out")
            implicitWidth: bar.theme.controlHeight
            implicitHeight: bar.theme.controlHeight
            focusPolicy: Qt.NoFocus
            onClicked: bar.editor.zoomOut()
        }
        Label {
            text: bar.editor.zoomText
            color: bar.theme.text
            horizontalAlignment: Text.AlignHCenter
            Layout.preferredWidth: bar.theme.controlHeight * 1.5
        }
        ToolButton {
            text: qsTr("+")
            Accessible.name: qsTr("Zoom in")
            implicitWidth: bar.theme.controlHeight
            implicitHeight: bar.theme.controlHeight
            focusPolicy: Qt.NoFocus
            onClicked: bar.editor.zoomIn()
        }
        IconButton {
            theme: bar.theme
            label: qsTr("Fit screen (Ctrl+0)")
            icon.source: "qrc:/icons/zoom-fit.svg"
            onClicked: bar.editor.zoomToFit()
        }

        Item {
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("Input: %1").arg(bar.theme.densityName)
            Accessible.name: qsTr("Switch between mouse and touch layout")
            implicitHeight: bar.theme.controlHeight
            focusPolicy: Qt.NoFocus
            onClicked: bar.theme.toggleDensity()
        }
    }
}
