import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The name column of a property row. A property that can be driven by a
// tag has a link button; a bound one shows its name in the accent colour
// and the binding as a tooltip.
RowLayout {
    id: label

    required property Theme theme
    required property string text
    property PropertyModel properties: null
    property int rowIndex: -1
    property bool bindable: false
    property string binding: ""

    Layout.preferredWidth: label.theme.propertyLabelWidth
    Layout.maximumWidth: label.theme.propertyLabelWidth
    Layout.alignment: Qt.AlignVCenter
    spacing: 0

    Label {
        Layout.fillWidth: true
        text: label.text
        elide: Text.ElideRight
        color: label.binding.length > 0 ? label.theme.accent : label.theme.textMuted
    }

    ToolButton {
        id: link

        visible: label.bindable && label.properties !== null
        implicitWidth: label.theme.controlHeight * 0.75
        implicitHeight: label.theme.controlHeight * 0.75
        display: AbstractButton.IconOnly
        icon.source: "qrc:/icons/link.svg"
        icon.width: label.theme.iconSize * 0.75
        icon.height: label.theme.iconSize * 0.75
        icon.color: label.binding.length > 0 ? label.theme.accent : label.theme.textMuted
        focusPolicy: Qt.NoFocus
        text: label.binding.length > 0 ? qsTr("Bound to %1").arg(label.binding) : qsTr("Bind to a tag")
        Accessible.name: link.text
        ToolTip.text: link.text
        ToolTip.visible: link.hovered
        ToolTip.delay: 400
        onClicked: label.properties.requestBinding(label.rowIndex)
    }
}
