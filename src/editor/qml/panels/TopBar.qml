pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Application bar: file menu and project title, tools, undo and redo, zoom,
// panel toggles and input density.
ToolBar {
    id: bar

    required property EditorController editor
    required property SimulationController simulation
    required property Theme theme
    required property DocumentController document
    required property AppSettings settings

    leftPadding: bar.theme.padding
    rightPadding: bar.theme.padding

    RowLayout {
        anchors.fill: parent
        spacing: bar.theme.spacing

        ToolButton {
            id: fileButton

            text: qsTr("File")
            implicitHeight: bar.theme.controlHeight
            focusPolicy: Qt.NoFocus
            onClicked: fileMenu.popup(fileButton, 0, fileButton.height)

            Menu {
                id: fileMenu

                MenuItem {
                    text: qsTr("New project")
                    onTriggered: bar.document.newProject()
                }
                MenuItem {
                    text: qsTr("Open…")
                    onTriggered: bar.document.open()
                }
                Menu {
                    id: recentMenu

                    title: qsTr("Open recent")
                    enabled: bar.document.recentProjects.length > 0

                    Instantiator {
                        model: bar.document.recentProjects
                        delegate: MenuItem {
                            required property string modelData

                            text: modelData
                            onTriggered: bar.document.openRecent(modelData)
                        }
                        onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => recentMenu.removeItem(object)
                    }
                }
                MenuSeparator {}
                MenuItem {
                    text: qsTr("Save")
                    onTriggered: bar.document.save()
                }
                MenuItem {
                    text: qsTr("Save as…")
                    onTriggered: bar.document.chooseSaveLocation()
                }
            }
        }

        ColumnLayout {
            spacing: 0
            Label {
                text: bar.document.title
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

        IconButton {
            theme: bar.theme
            label: qsTr("Snap to grid and objects")
            icon.source: "qrc:/icons/snap.svg"
            checked: bar.editor.snapEnabled
            onClicked: bar.editor.snapEnabled = !bar.editor.snapEnabled
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

        // Runtime simulation in a window of its own, as in TIA Portal.
        Button {
            id: simulateButton

            text: bar.simulation.running ? qsTr("Stop") : qsTr("Simulate")
            Accessible.name: bar.simulation.running ? qsTr("Stop the runtime simulation (Shift+F5)") : qsTr("Start the runtime simulation of this screen (F5)")
            ToolTip.text: simulateButton.Accessible.name
            ToolTip.visible: simulateButton.hovered
            ToolTip.delay: 600
            implicitHeight: bar.theme.controlHeight
            icon.source: bar.simulation.running ? "qrc:/icons/stop.svg" : "qrc:/icons/play.svg"
            icon.color: "transparent"
            icon.width: bar.theme.iconSize
            icon.height: bar.theme.iconSize
            focusPolicy: Qt.NoFocus
            onClicked: bar.simulation.toggle()
        }
        IconButton {
            visible: bar.simulation.running
            theme: bar.theme
            label: qsTr("Restart the simulation with your latest edits (F5)")
            icon.source: "qrc:/icons/redo.svg"
            onClicked: bar.simulation.restart()
        }


        Item {
            Layout.fillWidth: true
        }

        IconButton {
            theme: bar.theme
            label: qsTr("Toolbox and layers panel")
            icon.source: "qrc:/icons/panel-left.svg"
            checked: bar.settings.toolboxPanelVisible
            onClicked: bar.settings.toolboxPanelVisible = !bar.settings.toolboxPanelVisible
        }
        IconButton {
            theme: bar.theme
            label: qsTr("Bottom panel")
            icon.source: "qrc:/icons/panel-bottom.svg"
            checked: bar.settings.bottomPanelVisible
            onClicked: bar.settings.bottomPanelVisible = !bar.settings.bottomPanelVisible
        }
        IconButton {
            theme: bar.theme
            label: qsTr("Properties panel")
            icon.source: "qrc:/icons/panel-right.svg"
            checked: bar.settings.propertiesPanelVisible
            onClicked: bar.settings.propertiesPanelVisible = !bar.settings.propertiesPanelVisible
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
