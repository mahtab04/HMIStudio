import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Font property: the font dialog for everything, plus size, bold and italic
// directly in the row so the common changes need no dialog.
ColumnLayout {
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

    RowLayout {
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
            onClicked: dialog.open()
        }
    }

    RowLayout {
        spacing: row.theme.spacing / 2

        Item {
            Layout.preferredWidth: row.theme.propertyLabelWidth
        }
        SpinBox {
            Layout.fillWidth: true
            implicitHeight: row.theme.controlHeight
            from: 1
            to: 400
            editable: true
            enabled: !row.readOnly
            value: row.value.pixelSize > 0 ? row.value.pixelSize : row.value.pointSize
            Accessible.name: qsTr("Font size in pixels")
            onValueModified: row.properties.setFontPixelSize(row.index, value)
        }
        ToolButton {
            text: qsTr("B", "bold")
            font.bold: true
            checked: row.value.bold
            enabled: !row.readOnly
            implicitWidth: row.theme.controlHeight
            implicitHeight: row.theme.controlHeight
            focusPolicy: Qt.NoFocus
            Accessible.name: qsTr("Bold")
            onClicked: row.properties.setFontBold(row.index, !row.value.bold)
        }
        ToolButton {
            text: qsTr("I", "italic")
            font.italic: true
            checked: row.value.italic
            enabled: !row.readOnly
            implicitWidth: row.theme.controlHeight
            implicitHeight: row.theme.controlHeight
            focusPolicy: Qt.NoFocus
            Accessible.name: qsTr("Italic")
            onClicked: row.properties.setFontItalic(row.index, !row.value.italic)
        }
    }

    FontDialog {
        id: dialog

        title: row.name
        selectedFont: row.value
        onAccepted: row.properties.setValue(row.index, dialog.selectedFont)
    }
}
