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
        anchors.fill: parent
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

    RuntimeValueDialog {
        input: window.simulation.input
        theme: window.theme
    }

    Label {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        padding: window.theme.padding
        visible: !window.simulation.input.open && text.length > 0
        text: window.simulation.input.error
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
