pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Left panel: the object toolbox and the layers of the current screen.
// Toolbox: tap a tile to place with the next tap or drag on the canvas, or
// drag the tile onto the canvas.
Pane {
    id: panel

    required property EditorController editor
    required property Theme theme
    required property ToolboxModel toolbox
    required property ToolboxDragController toolboxDrag
    required property LayerModel layers

    padding: panel.theme.padding

    ColumnLayout {
        anchors.fill: parent
        spacing: panel.theme.spacing

        TabBar {
            id: tabs

            Layout.fillWidth: true
            TabButton {
                text: qsTr("Toolbox")
                implicitHeight: panel.theme.controlHeight
            }
            TabButton {
                text: qsTr("Layers")
                implicitHeight: panel.theme.controlHeight
            }
            TabButton {
                text: qsTr("Library")
                enabled: false
                implicitHeight: panel.theme.controlHeight
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            ColumnLayout {
                spacing: panel.theme.spacing

                Label {
                    text: qsTr("BASIC OBJECTS")
                    font: panel.theme.fontHeading
                    color: panel.theme.textMuted
                }

                GridView {
                    id: grid

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // Dragging a tile places an object, so the grid itself does not
                    // flick; the scroll bar scrolls it.
                    interactive: false
                    clip: true
                    cellWidth: grid.width / 4
                    cellHeight: panel.theme.toolboxTile
                    model: panel.toolbox
                    ScrollBar.vertical: ScrollBar {}

                    delegate: ToolboxTile {
                        width: grid.cellWidth
                        height: grid.cellHeight
                        editor: panel.editor
                        theme: panel.theme
                        toolboxDrag: panel.toolboxDrag
                    }
                }
            }

            ListView {
                id: layerList

                clip: true
                model: panel.layers
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: LayerRow {
                    width: layerList.width
                    layers: panel.layers
                    editor: panel.editor
                    theme: panel.theme
                }
            }
        }
    }
}
