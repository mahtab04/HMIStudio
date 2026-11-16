import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The tag of the object's process value, picked from the project's tags
// (or None), with a button for scaling and other transforms.
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property string binding
    required property var value
    required property var options
    required property bool mixed

    // "None" first, then the tags.
    readonly property var choices: [qsTr("None")].concat(row.options)

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
        displayText: row.mixed ? qsTr("Mixed") : row.value.length > 0 && row.options.indexOf(row.value) < 0 ? qsTr("%1 (missing)").arg(row.value) : picker.currentText
        Accessible.name: qsTr("Tag for %1").arg(row.binding)
        ToolTip.text: qsTr("The tag that drives %1").arg(row.binding)
        ToolTip.visible: picker.hovered
        ToolTip.delay: 600
        onActivated: choice => row.properties.setValue(row.index, choice === 0 ? "" : row.options[choice - 1])
    }

    IconButton {
        theme: row.theme
        label: qsTr("Scaling and transform…")
        icon.source: "qrc:/icons/link.svg"
        enabled: !row.mixed && row.value.length > 0
        onClicked: row.properties.requestBinding(row.index)
    }
}
