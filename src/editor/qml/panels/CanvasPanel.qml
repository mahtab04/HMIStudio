pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HmiStudio.Quick

// Centre panel, the work area: the "HMI tags" editor tab and one tab per
// screen; a screen tab shows rulers and the canvas.
ColumnLayout {
    id: panel

    required property EditorController editor
    required property Theme theme
    required property ScreenListModel screens
    required property TagModel tags
    required property SimulationController simulation

    // The tag table is open instead of a screen.
    property bool showingTags: false

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

            // Screen tabs. Tap shows a screen; right-click or long-press
            // opens its menu (rename, duplicate, delete).
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: panel.theme.controlHeight
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                contentHeight: availableHeight

                Row {
                    spacing: 2

                    ToolButton {
                        objectName: "tagsTab"
                        text: panel.tags.tagCount > 0 ? qsTr("HMI tags (%1)").arg(panel.tags.tagCount) : qsTr("HMI tags")
                        checked: panel.showingTags
                        implicitHeight: panel.theme.controlHeight
                        focusPolicy: Qt.NoFocus
                        onClicked: panel.showingTags = true
                    }
                    ToolSeparator {
                        implicitHeight: panel.theme.controlHeight
                    }

                    Repeater {
                        model: panel.screens

                        ToolButton {
                            id: tab

                            required property int index
                            required property string name
                            required property bool current

                            text: tab.name
                            checked: tab.current && !panel.showingTags
                            implicitHeight: panel.theme.controlHeight
                            focusPolicy: Qt.NoFocus
                            onClicked: {
                                panel.showingTags = false
                                panel.editor.setCurrentScreen(tab.index)
                            }
                            onPressAndHold: {
                                screenMenu.screenIndex = tab.index
                                screenMenu.popup(tab)
                            }

                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: {
                                    screenMenu.screenIndex = tab.index
                                    screenMenu.popup(tab)
                                }
                            }
                        }
                    }

                    IconButton {
                        theme: panel.theme
                        label: qsTr("Add screen")
                        icon.source: "qrc:/icons/plus.svg"
                        onClicked: {
                            panel.showingTags = false
                            panel.editor.addScreen()
                        }
                    }
                }
            }
            Label {
                visible: !panel.showingTags
                text: panel.editor.gridText
                color: panel.theme.textMuted
            }
            Label {
                visible: !panel.showingTags
                text: panel.editor.zoomText
                color: panel.theme.textMuted
            }
        }
    }

    TagsPanel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: panel.showingTags
        theme: panel.theme
        tags: panel.tags
        simulation: panel.simulation
    }

    RowLayout {
        Layout.fillWidth: true
        visible: !panel.showingTags
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
        visible: !panel.showingTags
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

            Menu {
                id: screenMenu

                // The screen the menu was opened for.
                property int screenIndex: 0

                MenuItem {
                    text: qsTr("Rename…")
                    onTriggered: renameDialog.open()
                }
                MenuItem {
                    text: qsTr("Duplicate")
                    onTriggered: panel.editor.duplicateScreen(screenMenu.screenIndex)
                }
                MenuItem {
                    text: qsTr("Delete")
                    enabled: panel.editor.canRemoveScreen
                    onTriggered: panel.editor.removeScreen(screenMenu.screenIndex)
                }
            }

            Dialog {
                id: renameDialog

                title: qsTr("Rename screen")
                width: panel.theme.sidePanelWidth * 1.2
                modal: true
                anchors.centerIn: Overlay.overlay
                standardButtons: Dialog.Ok | Dialog.Cancel
                onAboutToShow: nameField.text = panel.screens.nameAt(screenMenu.screenIndex)
                onAccepted: panel.editor.renameScreen(screenMenu.screenIndex, nameField.text)

                TextField {
                    id: nameField

                    width: renameDialog.availableWidth
                    implicitHeight: panel.theme.controlHeight
                    Accessible.name: qsTr("Screen name")
                    onAccepted: renameDialog.accept()
                }
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
