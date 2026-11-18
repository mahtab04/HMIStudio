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
    required property AlarmModel alarms
    required property SimulationController simulation

    // The tag table is open instead of a screen.
    property bool showingTags: false
    // The HMI alarms editor is open instead of a screen.
    property bool showingAlarms: false
    readonly property bool showingTable: panel.showingTags || panel.showingAlarms

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
                        onClicked: {
                            panel.showingAlarms = false
                            panel.showingTags = true
                        }
                    }
                    ToolButton {
                        objectName: "alarmsTab"
                        text: panel.alarms.alarmCount > 0 ? qsTr("HMI alarms (%1)").arg(panel.alarms.alarmCount) : qsTr("HMI alarms")
                        checked: panel.showingAlarms
                        implicitHeight: panel.theme.controlHeight
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            panel.showingTags = false
                            panel.showingAlarms = true
                        }
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
                            checked: tab.current && !panel.showingTable
                            implicitHeight: panel.theme.controlHeight
                            focusPolicy: Qt.NoFocus
                            onClicked: {
                                panel.showingTags = false
                                panel.showingAlarms = false
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
                            panel.showingAlarms = false
                            panel.editor.addScreen()
                        }
                    }
                }
            }
        }
    }

    AlarmsPanel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: panel.showingAlarms
        theme: panel.theme
        alarms: panel.alarms
    }

    TagsPanel {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: panel.showingTags
        theme: panel.theme
        tags: panel.tags
        simulation: panel.simulation
    }

    // The work area: the screen on a plain surround, no rulers, as in
    // TIA Portal's screen editor.
    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: !panel.showingTable
        spacing: 0

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

        // Zoom, bottom right, as in TIA Portal.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: panel.theme.controlHeight + panel.theme.spacing
            color: panel.theme.panel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: panel.theme.padding
                anchors.rightMargin: panel.theme.padding
                spacing: panel.theme.spacing

                Label {
                    text: panel.editor.gridText
                    color: panel.theme.textMuted
                }
                Item {
                    Layout.fillWidth: true
                }
                IconButton {
                    theme: panel.theme
                    label: qsTr("Fit screen (Ctrl+0)")
                    icon.source: "qrc:/icons/zoom-fit.svg"
                    onClicked: panel.editor.zoomToFit()
                }
                ToolButton {
                    text: qsTr("−")
                    Accessible.name: qsTr("Zoom out")
                    implicitWidth: panel.theme.controlHeight
                    implicitHeight: panel.theme.controlHeight
                    focusPolicy: Qt.NoFocus
                    onClicked: panel.editor.zoomOut()
                }
                Slider {
                    objectName: "zoomSlider"
                    Layout.preferredWidth: panel.theme.sidePanelWidth * 0.6
                    from: panel.editor.minimumZoomLevel
                    to: panel.editor.maximumZoomLevel
                    value: panel.editor.zoomLevel
                    focusPolicy: Qt.NoFocus
                    Accessible.name: qsTr("Zoom")
                    onMoved: panel.editor.zoomLevel = value
                }
                ToolButton {
                    text: qsTr("+")
                    Accessible.name: qsTr("Zoom in")
                    implicitWidth: panel.theme.controlHeight
                    implicitHeight: panel.theme.controlHeight
                    focusPolicy: Qt.NoFocus
                    onClicked: panel.editor.zoomIn()
                }
                ComboBox {
                    id: zoomBox

                    objectName: "zoomBox"
                    Layout.preferredWidth: panel.theme.controlHeight * 3
                    implicitHeight: panel.theme.controlHeight
                    model: panel.editor.zoomChoices
                    displayText: panel.editor.zoomText
                    focusPolicy: Qt.NoFocus
                    Accessible.name: qsTr("Zoom level")
                    onActivated: index => panel.editor.chooseZoom(index)
                }
            }
        }
    }
}
