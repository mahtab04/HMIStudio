pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import HmiStudio.Quick

// The runtime simulator: the simulated screen in a window of its own, sized
// to the screen (smaller if the desktop is), as the operator panel shows it.
// Closing the window stops the simulation.
Window {
    id: window

    required property SimulationController simulation
    required property Theme theme

    readonly property size screenSize: window.simulation.screenSize
    // The screen at 1:1 when it fits in most of the desktop, else smaller.
    readonly property real fit: Math.min(1, Screen.desktopAvailableWidth * 0.9 / Math.max(1, window.screenSize.width), Screen.desktopAvailableHeight * 0.85 / Math.max(1, window.screenSize.height))

    width: Math.round(window.screenSize.width * window.fit)
    height: Math.round(window.screenSize.height * window.fit)
    minimumWidth: 160
    minimumHeight: 120
    visible: true
    title: window.simulation.monitoring ? qsTr("%1 - PLC monitor").arg(window.simulation.screenTitle) : qsTr("%1 - Runtime simulation").arg(window.simulation.screenTitle)
    color: window.theme.window

    // Stopping destroys this window; not from inside its own close handler.
    onClosing: Qt.callLater(window.simulation.stop)

    RuntimeView {
        id: runtime
        objectName: "runtimeView"
        // Below the alarm line while it shows, so it covers nothing.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: alarmLine.visible ? alarmLine.bottom : parent.top
        Component.onCompleted: window.simulation.attachView(this)
        focus: true

        Rectangle {
            readonly property rect target: window.simulation.input.focusBounds
            x: (runtime.width - window.screenSize.width * runtime.zoom) / 2 + target.x * runtime.zoom - 3
            y: (runtime.height - window.screenSize.height * runtime.zoom) / 2 + target.y * runtime.zoom - 3
            width: target.width * runtime.zoom + 6
            height: target.height * runtime.zoom + 6
            visible: runtime.activeFocus && target.width > 0
            color: "transparent"
            border.color: window.theme.accent
            border.width: 2
            radius: 3
        }
    }

    // A popup screen over the current one: the screen behind is dimmed and
    // takes no input until the popup closes.
    Loader {
        anchors.fill: parent
        active: window.simulation.popupOpen
        sourceComponent: Item {
            // Dims the screen and swallows its input.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                Rectangle {
                    anchors.fill: parent
                    color: "#000000"
                    opacity: 0.4
                }
            }

            Rectangle {
                id: frame

                readonly property real titleHeight: window.theme.controlHeight
                // The popup at the same scale as the screen, smaller if it
                // does not fit.
                readonly property real popupScale: Math.min(runtime.zoom, (window.width - 24) / Math.max(1, window.simulation.popupSize.width), (window.height - 24 - frame.titleHeight) / Math.max(1, window.simulation.popupSize.height))

                objectName: "popupFrame"
                anchors.centerIn: parent
                width: Math.round(window.simulation.popupSize.width * frame.popupScale) + 4
                height: Math.round(window.simulation.popupSize.height * frame.popupScale) + frame.titleHeight + 4
                color: window.theme.panel
                border.color: window.theme.border
                border.width: 2
                radius: 4

                Rectangle {
                    id: titleBar

                    x: 2
                    y: 2
                    width: frame.width - 4
                    height: frame.titleHeight
                    color: window.theme.panelRaised

                    Label {
                        anchors.left: parent.left
                        anchors.leftMargin: window.theme.padding
                        anchors.verticalCenter: parent.verticalCenter
                        text: window.simulation.popupTitle
                        color: window.theme.text
                        font.bold: true
                    }
                    ToolButton {
                        objectName: "closePopupButton"
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: frame.titleHeight
                        implicitHeight: frame.titleHeight
                        text: "✕"
                        focusPolicy: Qt.NoFocus
                        Accessible.name: qsTr("Close popup")
                        onClicked: window.simulation.closePopup()
                    }
                }

                RuntimeView {
                    objectName: "popupView"
                    x: 2
                    y: 2 + frame.titleHeight
                    width: frame.width - 4
                    height: frame.height - frame.titleHeight - 4
                    focus: true
                    Component.onCompleted: {
                        window.simulation.attachPopupView(this)
                        forceActiveFocus()
                    }
                }
            }
        }
    }

    // The alarm line: the newest alarm to acknowledge, in its class colours.
    Rectangle {
        id: alarmLine

        objectName: "alarmLine"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: window.theme.controlHeight + 8
        visible: window.simulation.unacknowledgedAlarms > 0
        color: window.simulation.newestAlarmColor
        border.color: Qt.darker(window.simulation.newestAlarmColor, 1.4)

        Label {
            anchors.left: parent.left
            anchors.leftMargin: window.theme.padding
            anchors.right: ackButton.left
            anchors.rightMargin: window.theme.spacing
            anchors.verticalCenter: parent.verticalCenter
            text: window.simulation.unacknowledgedAlarms > 1 ? qsTr("%1   (+%2 more to acknowledge)").arg(window.simulation.newestAlarmText).arg(window.simulation.unacknowledgedAlarms - 1) : window.simulation.newestAlarmText
            color: window.simulation.newestAlarmTextColor
            font.bold: true
            elide: Text.ElideRight
        }
        Button {
            id: ackButton

            objectName: "acknowledgeAlarmButton"
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            implicitHeight: window.theme.controlHeight
            text: qsTr("ACK")
            focusPolicy: Qt.NoFocus
            Accessible.name: qsTr("Acknowledge this alarm")
            onClicked: window.simulation.acknowledgeNewestAlarm()
        }
    }

    RuntimeValueDialog {
        input: window.simulation.input
        theme: window.theme
    }
    RuntimeValueDialog {
        input: window.simulation.popupInput
        theme: window.theme
    }

    Label {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        padding: window.theme.padding
        visible: !window.simulation.input.open && !window.simulation.popupInput.open && text.length > 0
        text: window.simulation.popupOpen && window.simulation.popupInput.error.length > 0 ? window.simulation.popupInput.error : window.simulation.input.error
        color: window.theme.warning
        wrapMode: Text.WordWrap
        background: Rectangle {
            color: window.theme.window
        }
    }

    Shortcut {
        sequence: "F5"
        onActivated: window.simulation.restart()
    }
    Shortcut {
        sequence: "Shift+F5"
        onActivated: window.simulation.stop()
    }
}
