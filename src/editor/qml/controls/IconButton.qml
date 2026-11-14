import QtQuick
import QtQuick.Controls

// Icon-only tool button sized from the theme, with a tooltip and an
// accessible name taken from `label`.
ToolButton {
    id: control

    required property Theme theme
    required property string label

    implicitWidth: control.theme.controlHeight
    implicitHeight: control.theme.controlHeight
    display: AbstractButton.IconOnly
    icon.width: control.theme.iconSize
    icon.height: control.theme.iconSize
    icon.color: control.theme.text
    focusPolicy: Qt.NoFocus

    text: control.label
    Accessible.name: control.label
    ToolTip.text: control.label
    ToolTip.visible: control.hovered
    ToolTip.delay: 600
}
