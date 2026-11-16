import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The screen a screen window shows: one of the project's other screens, or
// None, as in WinCC Unified's screen window.
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property var value
    required property var options
    required property bool mixed

    // "None" first, then the screens.
    readonly property var choices: [qsTr("None")].concat(row.options)
    readonly property bool missing: row.value.length > 0 && row.options.indexOf(row.value) < 0

    spacing: row.theme.spacing / 2

    PropertyLabel {
        theme: row.theme
        text: row.name
    }

    ComboBox {
        id: picker

        Layout.fillWidth: true
        implicitHeight: row.theme.controlHeight
        model: row.choices
        currentIndex: row.mixed ? -1 : Math.max(0, row.options.indexOf(row.value) + 1)
        displayText: row.mixed ? qsTr("Mixed") : row.missing ? qsTr("%1 (missing)").arg(row.value) : picker.currentText
        Accessible.name: row.name
        onActivated: choice => row.properties.setValue(row.index, choice === 0 ? "" : row.options[choice - 1])
    }
}
