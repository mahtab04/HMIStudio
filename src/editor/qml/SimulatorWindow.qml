import QtQuick
import QtQuick.Window
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
    title: qsTr("%1 — Runtime simulation").arg(window.simulation.screenTitle)
    color: window.theme.window

    // Stopping destroys this window; not from inside its own close handler.
    onClosing: Qt.callLater(window.simulation.stop)

    RuntimeView {
        objectName: "runtimeView"
        anchors.fill: parent
        Component.onCompleted: window.simulation.attachView(this)
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
