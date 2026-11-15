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

                TextField {
                    Layout.fillWidth: true
                    implicitHeight: panel.theme.controlHeight
                    placeholderText: qsTr("Search objects")
                    text: panel.toolbox.filterText
                    Accessible.name: qsTr("Search objects")
                    onTextEdited: panel.toolbox.filterText = text
                }

                // Sections of tiles. Dragging a tile places an object, so the
                // list does not flick; the scroll bar and the wheel scroll it.
                ListView {
                    id: sections

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    interactive: false
                    clip: true
                    spacing: panel.theme.spacing
                    model: panel.toolbox
                    ScrollBar.vertical: ScrollBar {}

                    WheelHandler {
                        onWheel: event => sections.flick(0, event.angleDelta.y * 8)
                    }

                    delegate: ColumnLayout {
                        id: section

                        required property string title
                        required property var tiles

                        width: sections.width
                        spacing: panel.theme.spacing / 2

                        Label {
                            text: section.title.toUpperCase()
                            font: panel.theme.fontHeading
                            color: panel.theme.textMuted
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 4
                            columnSpacing: 0
                            rowSpacing: 0

                            Repeater {
                                model: section.tiles

                                ToolboxTile {
                                    required property var modelData

                                    Layout.fillWidth: true
                                    Layout.preferredWidth: sections.width / 4
                                    Layout.preferredHeight: panel.theme.toolboxTile
                                    key: modelData.key
                                    name: modelData.name
                                    iconSource: modelData.iconSource
                                    editor: panel.editor
                                    theme: panel.theme
                                    toolboxDrag: panel.toolboxDrag
                                }
                            }
                        }
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
