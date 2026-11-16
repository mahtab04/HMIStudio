import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Bottom panel: tags, and later alarms, scripts and output.
Pane {
    id: panel

    required property Theme theme
    required property TagModel tags
    required property SimulationController simulation

    padding: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabs

            Layout.fillWidth: true
            TabButton {
                text: panel.tags.tagCount > 0 ? qsTr("Tags (%1)").arg(panel.tags.tagCount) : qsTr("Tags")
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

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: panel.theme.spacing / 2
            currentIndex: tabs.currentIndex

            TagsPanel {
                theme: panel.theme
                tags: panel.tags
                simulation: panel.simulation
            }
            Label {
                text: qsTr("Alarms come in a later version.")
                color: panel.theme.textMuted
                verticalAlignment: Text.AlignTop
                leftPadding: panel.theme.padding
            }
            Label {
                text: qsTr("Scripts come in a later version.")
                color: panel.theme.textMuted
                verticalAlignment: Text.AlignTop
                leftPadding: panel.theme.padding
            }
            // Bindings that could not be applied while simulating.
            Label {
                text: panel.simulation.problems.length > 0 ? panel.simulation.problems.join("\n") : qsTr("No messages.")
                color: panel.simulation.problems.length > 0 ? panel.theme.warning : panel.theme.textMuted
                verticalAlignment: Text.AlignTop
                leftPadding: panel.theme.padding
                wrapMode: Text.WordWrap
            }
        }
    }
}
