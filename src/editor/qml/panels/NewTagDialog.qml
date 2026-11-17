pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog

    required property Theme theme
    required property TagModel tags

    objectName: "newTagDialog"
    title: qsTr("New HMI tag")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: dialog.theme.sidePanelWidth * 1.7
    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            implicitHeight: dialog.theme.controlHeight
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
    onOpened: tagName.forceActiveFocus()
    onClosed: tagName.clear()

    Connections {
        target: dialog.tags
        function onTagCreated() {
            dialog.close();
        }
    }

    GridLayout {
        width: dialog.availableWidth
        columns: 2
        columnSpacing: dialog.theme.spacing
        rowSpacing: dialog.theme.spacing

        Label {
            text: qsTr("Name")
        }
        TextField {
            id: tagName
            implicitHeight: dialog.theme.controlHeight
            objectName: "newTagName"
            Layout.fillWidth: true
            placeholderText: qsTr("TankLevel")
            Accessible.name: qsTr("HMI tag name")
        }
        Label {
            text: qsTr("Data type")
        }
        ComboBox {
            id: type
            implicitHeight: dialog.theme.controlHeight
            Layout.fillWidth: true
            model: dialog.tags.typeNames
            currentIndex: 2
            Accessible.name: qsTr("Tag data type")
        }
        Label {
            text: qsTr("Tag table")
        }
        TextField {
            id: tableName
            implicitHeight: dialog.theme.controlHeight
            Layout.fillWidth: true
            text: dialog.tags.tableFilter.length > 0 ? dialog.tags.tableFilter : "Default"
            Accessible.name: qsTr("Tag table")
        }
        Label {
            text: qsTr("Connection")
        }
        ComboBox {
            id: source
            implicitHeight: dialog.theme.controlHeight
            objectName: "newTagConnection"
            Layout.fillWidth: true
            model: dialog.tags.connections.names
            Accessible.name: qsTr("Tag connection")
        }
        Label {
            visible: source.currentIndex > 0
            text: qsTr("PLC address")
        }
        TextField {
            id: address
            implicitHeight: dialog.theme.controlHeight
            Layout.fillWidth: true
            visible: source.currentIndex > 0
            placeholderText: qsTr("DB1.DBD4 / Motor.Speed / hr0")
            Accessible.name: qsTr("PLC address")
        }
        Label {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: source.currentIndex === 0 ? qsTr("Internal tags hold values inside this HMI. Configure test signals in the Simulation view.") : qsTr("The address is read through the selected connection. Use Monitor PLCs to acquire values.")
            wrapMode: Text.WordWrap
            color: dialog.theme.textMuted
        }
        Label {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            visible: dialog.tags.message.length > 0
            text: dialog.tags.message
            wrapMode: Text.WordWrap
            color: dialog.theme.warning
        }
        Button {
            implicitHeight: dialog.theme.controlHeight
            objectName: "createTagButton"
            Layout.columnSpan: 2
            Layout.alignment: Qt.AlignRight
            text: qsTr("Create tag")
            enabled: tagName.text.trim().length > 0
            onClicked: dialog.tags.createTag(tagName.text, type.currentIndex, tableName.text, source.currentIndex, address.text)
        }
    }
}
