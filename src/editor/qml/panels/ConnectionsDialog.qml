pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: dialog

    required property Theme theme
    required property TagConnections connections

    objectName: "connectionsDialog"
    title: qsTr("PLC connections")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: dialog.theme.sidePanelWidth * 2
    footer: DialogButtonBox {
        Button {
            text: qsTr("Close")
            implicitHeight: dialog.theme.controlHeight
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
    onOpened: dialog.connections.edit(-1)

    Connections {
        target: dialog.connections
        function onSaved() {
            dialog.close();
        }
    }

    ColumnLayout {
        width: dialog.availableWidth
        spacing: dialog.theme.spacing

        RowLayout {
            Layout.fillWidth: true

            ComboBox {
                implicitHeight: dialog.theme.controlHeight
                Layout.fillWidth: true
                model: dialog.connections.entries
                textRole: "name"
                currentIndex: dialog.connections.selectedRow
                displayText: currentIndex < 0 ? qsTr("New connection") : currentText
                Accessible.name: qsTr("Connection to edit")
                onActivated: index => dialog.connections.edit(index)
            }
            Button {
                implicitHeight: dialog.theme.controlHeight
                text: qsTr("New")
                onClicked: dialog.connections.edit(-1)
            }
            Button {
                implicitHeight: dialog.theme.controlHeight
                text: qsTr("Remove")
                enabled: dialog.connections.selectedRow >= 0
                onClicked: dialog.connections.remove(dialog.connections.selectedRow)
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: dialog.theme.spacing
            rowSpacing: dialog.theme.spacing

            Label {
                text: qsTr("Name")
            }
            TextField {
                implicitHeight: dialog.theme.controlHeight
                objectName: "connectionName"
                Layout.fillWidth: true
                text: dialog.connections.draft.name ?? ""
                placeholderText: qsTr("PLC_Line1")
                Accessible.name: qsTr("Connection name")
                onTextEdited: dialog.connections.setDraftField("name", text)
            }
            Label {
                text: qsTr("Driver")
            }
            ComboBox {
                id: driver
                implicitHeight: dialog.theme.controlHeight
                Layout.fillWidth: true
                model: dialog.connections.driverNames
                currentIndex: dialog.connections.driverKeys.indexOf(dialog.connections.draft.driver)
                Accessible.name: qsTr("PLC driver")
                onActivated: index => dialog.connections.setDraftField("driver", dialog.connections.driverKeys[index])
            }
            Label {
                text: qsTr("Host / IP")
            }
            TextField {
                implicitHeight: dialog.theme.controlHeight
                Layout.fillWidth: true
                text: dialog.connections.draft.host ?? ""
                placeholderText: qsTr("192.168.0.10")
                Accessible.name: qsTr("PLC host or IP address")
                onTextEdited: dialog.connections.setDraftField("host", text)
            }
            Label {
                visible: driver.currentIndex === 0
                text: qsTr("Rack / slot")
            }
            RowLayout {
                visible: driver.currentIndex === 0
                SpinBox {
                    implicitHeight: dialog.theme.controlHeight
                    from: 0
                    to: 7
                    value: dialog.connections.draft.rack ?? 0
                    editable: true
                    Accessible.name: qsTr("S7 rack")
                    onValueModified: dialog.connections.setDraftField("rack", value)
                }
                SpinBox {
                    implicitHeight: dialog.theme.controlHeight
                    from: 0
                    to: 31
                    value: dialog.connections.draft.slot ?? 1
                    editable: true
                    Accessible.name: qsTr("S7 slot")
                    onValueModified: dialog.connections.setDraftField("slot", value)
                }
            }
            Label {
                visible: driver.currentIndex === 1
                text: qsTr("CIP route")
            }
            TextField {
                implicitHeight: dialog.theme.controlHeight
                Layout.fillWidth: true
                visible: driver.currentIndex === 1
                text: dialog.connections.draft.path ?? "1,0"
                Accessible.name: qsTr("CIP route")
                onTextEdited: dialog.connections.setDraftField("path", text)
            }
            Label {
                visible: driver.currentIndex === 2
                text: qsTr("Unit ID")
            }
            SpinBox {
                implicitHeight: dialog.theme.controlHeight
                visible: driver.currentIndex === 2
                from: 0
                to: 255
                value: dialog.connections.draft.unit ?? 1
                editable: true
                Accessible.name: qsTr("Modbus unit ID")
                onValueModified: dialog.connections.setDraftField("unit", value)
            }
            Label {
                text: qsTr("Native library")
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    implicitHeight: dialog.theme.controlHeight
                    Layout.fillWidth: true
                    text: dialog.connections.draft.library ?? ""
                    placeholderText: driver.currentIndex === 0 ? qsTr("snap7 beside the application") : qsTr("plctag beside the application")
                    Accessible.name: qsTr("Native driver library path")
                    onTextEdited: dialog.connections.setDraftField("library", text)
                }
                Button {
                    implicitHeight: dialog.theme.controlHeight
                    text: qsTr("Browse")
                    onClicked: libraryDialog.open()
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: driver.currentIndex === 0 ? qsTr("S7 uses absolute DB addresses. S7-1200/1500 requires compatible PUT/GET access and standard DB layout.") : driver.currentIndex === 1 ? qsTr("Logix scalar tags use BOOL, DINT and REAL addresses.") : qsTr("Modbus uses zero-based co/di bits and hr/ir registers. Real values use two registers in big-endian order.")
            wrapMode: Text.WordWrap
            color: dialog.theme.textMuted
        }
        Label {
            Layout.fillWidth: true
            visible: dialog.connections.message.length > 0
            text: dialog.connections.message
            wrapMode: Text.WordWrap
            color: dialog.theme.warning
        }
        Button {
            implicitHeight: dialog.theme.controlHeight
            Layout.alignment: Qt.AlignRight
            objectName: "saveConnectionButton"
            text: qsTr("Save connection")
            onClicked: dialog.connections.saveDraft()
        }
    }

    FileDialog {
        id: libraryDialog
        title: qsTr("Select native PLC library")
        fileMode: FileDialog.OpenFile
        onAccepted: dialog.connections.setLibraryFile(libraryDialog.selectedFile)
    }
}
