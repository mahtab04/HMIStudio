pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// On-screen number pad for the touch layout, so numbers can be entered
// without a keyboard. Shows `initialText` when opened and emits `entered`
// with the typed text on OK.
Popup {
    id: keypad

    required property Theme theme
    property string initialText

    signal entered(string text)

    modal: true
    focus: true
    padding: keypad.theme.padding

    onAboutToShow: entry.text = keypad.initialText

    ColumnLayout {
        spacing: keypad.theme.spacing

        TextField {
            id: entry

            Layout.fillWidth: true
            implicitHeight: keypad.theme.touchTarget
            horizontalAlignment: Text.AlignRight
            font: keypad.theme.fontHeading
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            Accessible.name: qsTr("Value")
        }

        GridLayout {
            columns: 3
            rowSpacing: keypad.theme.spacing
            columnSpacing: keypad.theme.spacing

            Repeater {
                model: ["7", "8", "9", "4", "5", "6", "1", "2", "3", "-", "0", Qt.locale().decimalPoint]

                Button {
                    required property string modelData

                    text: modelData
                    implicitWidth: keypad.theme.touchTarget * 1.4
                    implicitHeight: keypad.theme.touchTarget
                    focusPolicy: Qt.NoFocus
                    onClicked: entry.insert(entry.cursorPosition, modelData)
                }
            }

            Button {
                text: qsTr("⌫")
                Accessible.name: qsTr("Delete last character")
                implicitWidth: keypad.theme.touchTarget * 1.4
                implicitHeight: keypad.theme.touchTarget
                focusPolicy: Qt.NoFocus
                onClicked: entry.remove(entry.cursorPosition - 1, entry.cursorPosition)
            }
            Button {
                text: qsTr("Cancel")
                implicitWidth: keypad.theme.touchTarget * 1.4
                implicitHeight: keypad.theme.touchTarget
                focusPolicy: Qt.NoFocus
                onClicked: keypad.close()
            }
            Button {
                text: qsTr("OK")
                highlighted: true
                implicitWidth: keypad.theme.touchTarget * 1.4
                implicitHeight: keypad.theme.touchTarget
                focusPolicy: Qt.NoFocus
                onClicked: {
                    keypad.entered(entry.text)
                    keypad.close()
                }
            }
        }
    }
}
