import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Yes/no property: a check box, partly checked for a mixed selection.
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property var value
    required property bool mixed
    required property bool readOnly

    spacing: row.theme.spacing / 2

    PropertyLabel {
        theme: row.theme
        text: row.name
    }

    CheckBox {
        Layout.fillWidth: true
        implicitHeight: row.theme.controlHeight
        checkState: row.mixed ? Qt.PartiallyChecked : row.value ? Qt.Checked : Qt.Unchecked
        enabled: !row.readOnly
        focusPolicy: Qt.NoFocus
        Accessible.name: row.name
        // A click on a mixed state sets every object to "yes".
        onClicked: row.properties.setValue(row.index, row.mixed || !row.value)
    }
}
