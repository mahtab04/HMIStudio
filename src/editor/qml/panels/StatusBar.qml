import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Status bar: screen, file notes ("Saved ..."), selection, pointer position
// and zoom.
ToolBar {
    id: bar

    required property EditorController editor
    required property Theme theme
    required property DocumentController document

    leftPadding: bar.theme.padding
    rightPadding: bar.theme.padding

    RowLayout {
        anchors.fill: parent
        spacing: bar.theme.padding * 2

        Label {
            text: bar.editor.screenTitle
            color: bar.theme.textMuted
            font: bar.theme.fontSmall
        }
        Label {
            Layout.fillWidth: true
            text: bar.document.statusText
            elide: Text.ElideMiddle
            color: bar.theme.good
            font: bar.theme.fontSmall
        }
        Label {
            text: bar.editor.selectionText
            color: bar.theme.textMuted
            font: bar.theme.fontSmall
        }
        Label {
            text: bar.editor.cursorText
            color: bar.theme.textMuted
            font: bar.theme.fontSmall
        }
    }
}
