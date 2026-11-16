import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Graphic property: the imported file's name, a button to choose an image
// or SVG file, and one to remove the graphic.
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property bool bindable
    required property string binding
    required property string text
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

    Button {
        Layout.fillWidth: true
        implicitHeight: row.theme.controlHeight
        enabled: !row.readOnly
        focusPolicy: Qt.NoFocus
        text: row.mixed ? qsTr("Mixed") : row.text
        Accessible.name: qsTr("Choose %1").arg(row.name)
        onClicked: dialog.open()
    }
    IconButton {
        theme: row.theme
        label: qsTr("Remove graphic")
        icon.source: "qrc:/icons/delete.svg"
        enabled: !row.readOnly
        onClicked: row.properties.setValue(row.index, "")
    }

    FileDialog {
        id: dialog

        title: qsTr("Choose a graphic")
        nameFilters: [qsTr("Images and SVG (*.svg *.png *.jpg *.jpeg *.bmp *.gif *.webp)"),
                      qsTr("All files (*)")]
        onAccepted: row.properties.importAsset(row.index, dialog.selectedFile, PropertyModel.AskAboutSvg)
    }
}
