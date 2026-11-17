pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HmiStudio.Quick

Dialog {
    id: dialog

    required property RuntimeInputHandler input
    required property Theme theme

    anchors.centerIn: Overlay.overlay
    width: Math.min(420, parent.width - 24)
    modal: true
    visible: dialog.input.open
    title: dialog.input.title
    closePolicy: Popup.CloseOnEscape
    onRejected: dialog.input.dismiss()

    ColumnLayout {
        anchors.fill: parent
        spacing: dialog.theme.spacing

        Label {
            Layout.fillWidth: true
            text: dialog.input.choices.length > 0 ? qsTr("Select the process value") : qsTr("Enter the process value, then Apply")
            wrapMode: Text.WordWrap
        }
        ComboBox {
            objectName: "runtimeChoice"
            Layout.fillWidth: true
            implicitHeight: dialog.theme.controlHeight
            visible: dialog.input.choices.length > 0
            model: dialog.input.choices
            currentIndex: dialog.input.selectedIndex
            Accessible.name: qsTr("Process value")
            onActivated: index => dialog.input.choose(index)
        }
        TextField {
            id: entry
            objectName: "runtimeValueEntry"
            Layout.fillWidth: true
            implicitHeight: dialog.theme.controlHeight
            visible: dialog.input.choices.length === 0
            text: dialog.input.text
            focus: dialog.visible && dialog.input.choices.length === 0
            onActiveFocusChanged: entry.selectAll()
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            Accessible.name: qsTr("Process value")
            onTextEdited: dialog.input.setText(text)
            onAccepted: dialog.input.submit()
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            visible: dialog.input.choices.length === 0
            Repeater {
                model: ["7", "8", "9", "4", "5", "6", "1", "2", "3", "sign", "0", "decimal", "clear", "backspace"]
                Button {
                    required property string modelData
                    Layout.fillWidth: true
                    implicitHeight: dialog.theme.controlHeight
                    text: modelData === "sign" ? "+/-" : modelData === "decimal" ? Qt.locale().decimalPoint : modelData === "clear" ? qsTr("Clear") : modelData === "backspace" ? qsTr("Delete") : modelData
                    onClicked: dialog.input.inputKey(modelData)
                }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: dialog.input.error
            color: dialog.theme.warning
            wrapMode: Text.WordWrap
        }
    }
    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            implicitHeight: dialog.theme.controlHeight
            onClicked: dialog.input.dismiss()
        }
        Button {
            objectName: "runtimeApply"
            text: qsTr("Apply")
            visible: dialog.input.choices.length === 0
            implicitHeight: dialog.theme.controlHeight
            onClicked: dialog.input.submit()
        }
    }
}
