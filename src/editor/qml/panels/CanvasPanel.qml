import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HmiStudio.Quick

// Centre panel: screen tab, rulers and the canvas.
ColumnLayout {
    id: panel

    required property EditorController editor
    required property Theme theme

    spacing: 0

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: panel.theme.controlHeight + panel.theme.spacing
        color: panel.theme.panel

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: panel.theme.padding
            anchors.rightMargin: panel.theme.padding
            spacing: panel.theme.spacing

            TabBar {
                TabButton {
                    text: panel.editor.screenTitle
                    implicitHeight: panel.theme.controlHeight
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                text: panel.editor.gridText
                color: panel.theme.textMuted
            }
            Label {
                text: panel.editor.zoomText
                color: panel.theme.textMuted
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 0

        Rectangle {
            Layout.preferredWidth: panel.theme.rulerSize
            Layout.preferredHeight: panel.theme.rulerSize
            color: panel.theme.rulerBackground
        }
        RulerItem {
            Layout.fillWidth: true
            Layout.preferredHeight: panel.theme.rulerSize
            canvas: screenCanvas
            orientation: Qt.Horizontal
            background: panel.theme.rulerBackground
            tickColor: panel.theme.rulerTick
            textColor: panel.theme.textMuted
            font: panel.theme.fontSmall
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        RulerItem {
            Layout.preferredWidth: panel.theme.rulerSize
            Layout.fillHeight: true
            canvas: screenCanvas
            orientation: Qt.Vertical
            background: panel.theme.rulerBackground
            tickColor: panel.theme.rulerTick
            textColor: panel.theme.textMuted
            font: panel.theme.fontSmall
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: panel.theme.canvasBackground

            CanvasItem {
                id: screenCanvas

                anchors.fill: parent
                focus: true
                gridColor: panel.theme.grid
                selectionColor: panel.theme.selection
                handleFillColor: panel.theme.handleFill
                rubberBandColor: panel.theme.rubberBand
                guideColor: panel.theme.guide
                Accessible.role: Accessible.Canvas
                Accessible.name: qsTr("Screen canvas")

                Component.onCompleted: panel.editor.attachCanvas(screenCanvas)
            }

            // Width × height (and angle) of the selection, under it.
            Label {
                readonly property rect bounds: screenCanvas.selectionBounds

                objectName: "sizeLabel"
                visible: panel.editor.sizeText.length > 0
                x: Math.max(0, Math.min(parent.width - width,
                                        bounds.x + (bounds.width - width) / 2))
                y: Math.min(parent.height - height,
                            bounds.y + bounds.height + panel.theme.spacing)
                padding: 2
                leftPadding: panel.theme.spacing
                rightPadding: panel.theme.spacing
                text: panel.editor.sizeText
                font: panel.theme.fontSmall
                color: panel.theme.accentText
                background: Rectangle {
                    color: panel.theme.selection
                    radius: panel.theme.radius
                }
            }

            // Touch layout only: actions for the selection, above it (clear
            // of the rotation handle) or below it when there is no room.
            SelectionToolbar {
                readonly property rect bounds: screenCanvas.selectionBounds
                readonly property real above: bounds.y - height - panel.theme.touchTarget

                objectName: "selectionToolbar"
                editor: panel.editor
                theme: panel.theme
                menu: contextMenu
                visible: panel.theme.comfortable && panel.editor.hasSelection
                         && !screenCanvas.previewActive
                x: Math.max(0, Math.min(parent.width - width,
                                        bounds.x + (bounds.width - width) / 2))
                y: above >= 0 ? above
                              : Math.min(parent.height - height,
                                         bounds.y + bounds.height + panel.theme.touchTarget)
            }

            CanvasContextMenu {
                id: contextMenu

                objectName: "contextMenu"
                editor: panel.editor
            }

            Connections {
                target: panel.editor

                function onContextMenuRequested(viewPos: point) {
                    contextMenu.popup(screenCanvas, viewPos)
                }
            }
        }
    }
}
