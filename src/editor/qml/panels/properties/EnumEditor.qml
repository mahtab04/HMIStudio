import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Enumerated property: a drop-down of the allowed values.
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property bool bindable
    required property string binding
    required property var value
    required property var options
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

    ComboBox {
        Layout.fillWidth: true
        implicitHeight: row.theme.controlHeight
        model: row.options
        currentIndex: row.mixed ? -1 : row.value
        displayText: row.mixed ? qsTr("Mixed") : currentText
        enabled: !row.readOnly
        Accessible.name: row.name
        onActivated: choice => row.properties.setValue(row.index, choice)
    }
}
