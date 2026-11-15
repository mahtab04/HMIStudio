import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Colour property: a swatch button that opens the colour dialog.
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property string text
    required property var value
    required property bool mixed
    required property bool readOnly

    spacing: row.theme.spacing / 2

    PropertyLabel {
        theme: row.theme
        text: row.name
    }

    Button {
        Layout.fillWidth: true
        implicitHeight: row.theme.controlHeight
        enabled: !row.readOnly
        focusPolicy: Qt.NoFocus
        text: row.mixed ? qsTr("Mixed") : row.text
        Accessible.name: qsTr("%1: %2").arg(row.name).arg(text)
        icon.source: "qrc:/icons/rectangle.svg"
        icon.color: row.mixed ? row.theme.textMuted : row.value
        onClicked: dialog.open()
    }

    ColorDialog {
        id: dialog

        title: row.name
        selectedColor: row.value
        options: ColorDialog.ShowAlphaChannel
        onAccepted: row.properties.setValue(row.index, dialog.selectedColor)
    }
}
