import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The name column of a property row.
Label {
    id: label

    required property Theme theme

    Layout.preferredWidth: label.theme.propertyLabelWidth
    Layout.alignment: Qt.AlignVCenter
    elide: Text.ElideRight
    color: label.theme.textMuted
}
