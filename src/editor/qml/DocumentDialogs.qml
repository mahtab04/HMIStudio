import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

// The dialogs of DocumentController: open and save-as file dialogs, the
// unsaved-changes question, recovery after a crash, and errors. The
// controller decides when each is needed; this file only shows them and
// passes the answer back.
Item {
    id: dialogs

    required property DocumentController document
    required property Theme theme

    FileDialog {
        id: openDialog

        title: qsTr("Open project")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("HMI Studio project (project.json)")]
        onAccepted: dialogs.document.openFolder(openDialog.selectedFile)
    }

    FileDialog {
        id: saveDialog

        title: qsTr("Save project as")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("HMI Studio project (*.hmiproj)")]
        onAccepted: dialogs.document.saveAs(saveDialog.selectedFile)
        onRejected: dialogs.document.cancelPending()
    }

    Dialog {
        id: confirmDialog

        title: qsTr("Unsaved changes")
        width: dialogs.theme.sidePanelWidth * 1.5
        modal: true
        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        onAccepted: dialogs.document.saveChanges()
        onDiscarded: {
            confirmDialog.close()
            dialogs.document.discardChanges()
        }
        onRejected: dialogs.document.cancelPending()

        Label {
            width: dialogs.theme.sidePanelWidth * 1.3
            text: qsTr("Save the changes to %1 first?").arg(dialogs.document.title)
            wrapMode: Text.WordWrap
        }
    }

    Dialog {
        id: recoveryDialog

        title: qsTr("Recover unsaved changes")
        width: dialogs.theme.sidePanelWidth * 1.5
        modal: true
        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        standardButtons: Dialog.Ok | Dialog.Discard
        onAccepted: dialogs.document.recover()
        onDiscarded: {
            recoveryDialog.close()
            dialogs.document.discardRecovery()
        }

        Label {
            width: dialogs.theme.sidePanelWidth * 1.3
            text: qsTr("HMI Studio did not close normally last time. Recover the changes that were not saved?")
            wrapMode: Text.WordWrap
        }
    }

    MessageDialog {
        id: errorDialog

        title: qsTr("HMI Studio")
        text: dialogs.document.errorText
        buttons: MessageDialog.Ok
    }

    Connections {
        target: dialogs.document

        function onOpenDialogRequested() {
            openDialog.open()
        }
        function onSaveAsDialogRequested() {
            saveDialog.open()
        }
        function onConfirmRequested() {
            confirmDialog.open()
        }
        function onErrorOccurred() {
            errorDialog.open()
        }
    }

    Component.onCompleted: if (dialogs.document.recoveryAvailable) recoveryDialog.open()
}
