import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Bottom panel for tags, alarms, scripts and output (later phases).
Pane {
    id: panel

    required property Theme theme

    padding: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            Layout.fillWidth: true
            TabButton {
                text: qsTr("Tags")
                implicitHeight: panel.theme.controlHeight
            }
            TabButton {
                text: qsTr("Alarms")
                implicitHeight: panel.theme.controlHeight
            }
            TabButton {
                text: qsTr("Scripts")
                implicitHeight: panel.theme.controlHeight
            }
            TabButton {
                text: qsTr("Output")
                implicitHeight: panel.theme.controlHeight
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: panel.theme.padding
            text: qsTr("Tags and output are not available yet.")
            color: panel.theme.textMuted
            verticalAlignment: Text.AlignTop
        }
    }
}
