import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Number property: a text field with − and + steppers. Holding a stepper
// repeats, and the whole hold is one undo step. In the touch layout tapping
// the field opens the number pad instead of the keyboard.
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property bool bindable
    required property string binding
    required property string text
    required property bool mixed
    required property bool readOnly

    spacing: row.theme.spacing / 2

    PropertyLabel {
        theme: row.theme
        text: row.name
        properties: row.properties
        rowIndex: row.index
        bindable: row.bindable
        binding: row.binding
    }

    TextField {
        id: field

        Layout.fillWidth: true
        implicitHeight: row.theme.controlHeight
        text: row.text
        placeholderText: row.mixed ? qsTr("Mixed") : ""
        readOnly: row.readOnly || row.theme.comfortable
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
        Accessible.name: row.name
        onEditingFinished: row.properties.setValue(row.index, field.text)

        TapHandler {
            enabled: row.theme.comfortable && !row.readOnly
            onTapped: keypad.open()
        }

        NumberKeypad {
            id: keypad

            theme: row.theme
            initialText: row.text
            anchors.centerIn: Overlay.overlay
            onEntered: text => row.properties.setValue(row.index, text)
        }
    }

    IconButton {
        theme: row.theme
        label: qsTr("Decrease %1").arg(row.name)
        icon.source: "qrc:/icons/minus.svg"
        visible: !row.readOnly
        autoRepeat: true
        onDownChanged: down ? row.properties.beginEdit() : row.properties.endEdit()
        onPressed: row.properties.stepValue(row.index, -1)
    }
    IconButton {
        theme: row.theme
        label: qsTr("Increase %1").arg(row.name)
        icon.source: "qrc:/icons/plus.svg"
        visible: !row.readOnly
        autoRepeat: true
        onDownChanged: down ? row.properties.beginEdit() : row.properties.endEdit()
        onPressed: row.properties.stepValue(row.index, 1)
    }

}
