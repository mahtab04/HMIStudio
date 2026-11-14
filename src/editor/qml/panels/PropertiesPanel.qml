import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Right panel. Property editing arrives in phase 4; for now it shows the
// selection.
Pane {
    id: panel

    required property EditorController editor
    required property Theme theme

    padding: panel.theme.padding

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: panel.theme.spacing

        Label {
            text: qsTr("PROPERTIES")
            font: panel.theme.fontHeading
            color: panel.theme.textMuted
        }
        Label {
            text: panel.editor.selectionText
            color: panel.theme.text
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Property editing is not available yet.")
            wrapMode: Text.WordWrap
            color: panel.theme.textMuted
        }
    }
}
