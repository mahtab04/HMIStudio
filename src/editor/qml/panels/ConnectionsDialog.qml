pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// PLC connections: a PLC is a name, a PLC type and an IP address, with a
// Test button. Rack and slot, the CIP route, the Modbus unit and (for
// Allen-Bradley) the driver library have defaults and wait under "More
// settings". A test PLC on this computer can stand in for a real one.
Dialog {
    id: dialog

    required property Theme theme
    required property TagConnections connections

    property bool more: false

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
    onOpened: {
        dialog.more = false
        dialog.connections.edit(-1)
    }

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
                displayText: currentIndex < 0 ? qsTr("New PLC") : currentText
                Accessible.name: qsTr("PLC to edit")
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
                placeholderText: qsTr("PLC_1")
                Accessible.name: qsTr("PLC name")
                onTextEdited: dialog.connections.setDraftField("name", text)
            }
            Label {
                text: qsTr("PLC type")
            }
            ComboBox {
                id: plcType

                objectName: "plcType"
                implicitHeight: dialog.theme.controlHeight
                Layout.fillWidth: true
                model: dialog.connections.plcTypeNames
                currentIndex: dialog.connections.plcType
                Accessible.name: qsTr("PLC type")
                onActivated: index => dialog.connections.setPlcType(index)
            }
            Label {
                text: qsTr("IP address")
            }
            RowLayout {
                Layout.fillWidth: true

                TextField {
                    objectName: "connectionAddress"
                    implicitHeight: dialog.theme.controlHeight
                    Layout.fillWidth: true
                    text: dialog.connections.address
                    placeholderText: qsTr("192.168.0.1   (add :port if not the usual one)")
                    Accessible.name: qsTr("PLC IP address")
                    onTextEdited: dialog.connections.setAddress(text)
                }
                Button {
                    objectName: "testConnectionButton"
                    implicitHeight: dialog.theme.controlHeight
                    text: dialog.connections.testing ? qsTr("Testing…") : qsTr("Test")
                    enabled: !dialog.connections.testing
                    onClicked: dialog.connections.testConnection()
                }
            }
        }

        Label {
            objectName: "testResult"
            Layout.fillWidth: true
            visible: dialog.connections.testResult.length > 0
            text: dialog.connections.testResult
            wrapMode: Text.WordWrap
            color: dialog.connections.testPassed ? dialog.theme.good : dialog.connections.testing ? dialog.theme.textMuted : dialog.theme.warning
        }
        Label {
            Layout.fillWidth: true
            text: dialog.connections.addressHint
            wrapMode: Text.WordWrap
            color: dialog.theme.textMuted
        }

        // Rarely needed: the defaults fit most PLCs.
        Button {
            objectName: "moreSettingsButton"
            flat: true
            implicitHeight: dialog.theme.controlHeight
            text: dialog.more ? qsTr("▾ More settings") : qsTr("▸ More settings")
            onClicked: dialog.more = !dialog.more
        }
        GridLayout {
            Layout.fillWidth: true
            visible: dialog.more
            columns: 2
            columnSpacing: dialog.theme.spacing
            rowSpacing: dialog.theme.spacing

            Label {
                visible: plcType.currentIndex <= 1
                text: qsTr("Rack / slot")
            }
            RowLayout {
                visible: plcType.currentIndex <= 1
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
                visible: plcType.currentIndex === 3
                text: qsTr("CIP route")
            }
            TextField {
                implicitHeight: dialog.theme.controlHeight
                Layout.fillWidth: true
                visible: plcType.currentIndex === 3
                text: dialog.connections.draft.path ?? "1,0"
                Accessible.name: qsTr("CIP route")
                onTextEdited: dialog.connections.setDraftField("path", text)
            }
            Label {
                visible: plcType.currentIndex === 2
                text: qsTr("Unit ID")
            }
            SpinBox {
                implicitHeight: dialog.theme.controlHeight
                visible: plcType.currentIndex === 2
                from: 0
                to: 255
                value: dialog.connections.draft.unit ?? 1
                editable: true
                Accessible.name: qsTr("Modbus unit ID")
                onValueModified: dialog.connections.setDraftField("unit", value)
            }
            Label {
                visible: plcType.currentIndex === 3
                text: qsTr("Driver library")
            }
            RowLayout {
                visible: plcType.currentIndex === 3
                Layout.fillWidth: true
                TextField {
                    implicitHeight: dialog.theme.controlHeight
                    Layout.fillWidth: true
                    text: dialog.connections.draft.library ?? ""
                    placeholderText: qsTr("plctag, beside the application")
                    Accessible.name: qsTr("Driver library path")
                    onTextEdited: dialog.connections.setDraftField("library", text)
                }
                Button {
                    implicitHeight: dialog.theme.controlHeight
                    text: qsTr("Browse")
                    onClicked: libraryDialog.open()
                }
            }
        }

        // A test PLC on this computer, instead of hardware.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: testPlc.implicitHeight + 2 * dialog.theme.spacing
            color: dialog.theme.panelRaised
            radius: 4

            ColumnLayout {
                id: testPlc

                anchors.fill: parent
                anchors.margins: dialog.theme.spacing
                spacing: dialog.theme.spacing

                Label {
                    objectName: "testPlcStatus"
                    Layout.fillWidth: true
                    text: dialog.connections.testPlcStatus
                    wrapMode: Text.WordWrap
                    color: dialog.connections.testPlcRunning ? dialog.theme.good : dialog.theme.textMuted
                }
                RowLayout {
                    Layout.fillWidth: true

                    Button {
                        objectName: "testPlcButton"
                        implicitHeight: dialog.theme.controlHeight
                        text: dialog.connections.testPlcRunning ? qsTr("Stop test PLC") : qsTr("Start test PLC")
                        onClicked: dialog.connections.testPlcRunning ? dialog.connections.stopTestPlc() : dialog.connections.startTestPlc()
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Button {
                        objectName: "useTestPlcButton"
                        implicitHeight: dialog.theme.controlHeight
                        text: qsTr("Use as S7 PLC")
                        onClicked: dialog.connections.useTestPlc(false)
                    }
                    Button {
                        implicitHeight: dialog.theme.controlHeight
                        text: qsTr("Use as Modbus")
                        onClicked: dialog.connections.useTestPlc(true)
                    }
                }
            }
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
            text: qsTr("Save")
            onClicked: dialog.connections.saveDraft()
        }
    }

    FileDialog {
        id: libraryDialog
        title: qsTr("Select the libplctag library")
        fileMode: FileDialog.OpenFile
        onAccepted: dialog.connections.setLibraryFile(libraryDialog.selectedFile)
    }
}
