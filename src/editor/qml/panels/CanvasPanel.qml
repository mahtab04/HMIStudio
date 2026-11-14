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
                Accessible.role: Accessible.Canvas
                Accessible.name: qsTr("Screen canvas")

                Component.onCompleted: panel.editor.attachCanvas(screenCanvas)
            }
        }
    }
}
