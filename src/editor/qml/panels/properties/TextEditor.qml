import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Text property; applied when editing finishes (Enter or leaving the field).
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property string text
    required property bool mixed
    required property bool readOnly

    spacing: row.theme.spacing / 2

    PropertyLabel {
        theme: row.theme
        text: row.name
    }

    TextField {
        id: field

        Layout.fillWidth: true
        implicitHeight: row.theme.controlHeight
        text: row.text
        placeholderText: row.mixed ? qsTr("Mixed") : ""
        readOnly: row.readOnly
        selectByMouse: true
        Accessible.name: row.name
        onEditingFinished: row.properties.setValue(row.index, field.text)
    }
}
