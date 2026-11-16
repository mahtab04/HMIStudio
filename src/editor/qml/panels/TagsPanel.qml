pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.qmlmodels

// The Tags tab: the project's tags as an editable table with search, add,
// delete (with a warning for tags in use), "where used" and CSV import and
// export. While simulating the Value column is live, and typing a value
// writes it to the tag.
Item {
    id: panel

    required property Theme theme
    required property TagModel tags
    required property SimulationController simulation

    property int currentRow: -1
    property string message

    function deleteCurrent() {
        if (panel.currentRow < 0)
            return
        if (panel.tags.usageCount(panel.currentRow) > 0) {
            deleteDialog.uses = panel.tags.whereUsed(panel.currentRow)
            deleteDialog.open()
            return
        }
        panel.tags.removeTag(panel.currentRow)
        panel.currentRow = -1
    }

    Connections {
        target: panel.tags

        function onProblem(message: string) {
            panel.message = message
        }
        function onModelReset() {
            if (panel.currentRow >= panel.tags.rowCount())
                panel.currentRow = -1
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: panel.theme.spacing / 2

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: panel.theme.padding
            Layout.rightMargin: panel.theme.padding
            spacing: panel.theme.spacing

            TextField {
                Layout.preferredWidth: panel.theme.sidePanelWidth
                implicitHeight: panel.theme.controlHeight
                placeholderText: qsTr("Search tags")
                text: panel.tags.filterText
                onTextEdited: panel.tags.filterText = text
            }
            Button {
                text: qsTr("Add tag")
                implicitHeight: panel.theme.controlHeight
                focusPolicy: Qt.NoFocus
                onClicked: {
                    panel.message = ""
                    panel.currentRow = panel.tags.addTag()
                    table.positionViewAtRow(Math.max(0, panel.currentRow), TableView.Contain)
                }
            }
            Button {
                text: qsTr("Delete")
                implicitHeight: panel.theme.controlHeight
                enabled: panel.currentRow >= 0
                focusPolicy: Qt.NoFocus
                onClicked: panel.deleteCurrent()
            }
            Button {
                text: qsTr("Where used")
                implicitHeight: panel.theme.controlHeight
                enabled: panel.currentRow >= 0
                focusPolicy: Qt.NoFocus
                onClicked: {
                    usesDialog.uses = panel.tags.whereUsed(panel.currentRow)
                    usesDialog.open()
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                visible: panel.simulation.running
                text: qsTr("● Simulating")
                color: panel.theme.good
            }
            Button {
                text: qsTr("Import CSV…")
                implicitHeight: panel.theme.controlHeight
                focusPolicy: Qt.NoFocus
                onClicked: importDialog.open()
            }
            Button {
                text: qsTr("Export CSV…")
                implicitHeight: panel.theme.controlHeight
                enabled: panel.tags.tagCount > 0
                focusPolicy: Qt.NoFocus
                onClicked: exportDialog.open()
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: panel.theme.padding
            visible: panel.message.length > 0
            text: panel.message
            color: panel.theme.warning
            font: panel.theme.fontSmall
            elide: Text.ElideRight
        }

        HorizontalHeaderView {
            id: header

            Layout.fillWidth: true
            syncView: table
            clip: true
            textRole: "display"
        }

        TableView {
            id: table

            // Name, type, unit, minimum, maximum, simulation, period, comment, value.
            readonly property var weights: [1.6, 1.0, 0.7, 0.8, 0.8, 1.0, 0.8, 2.0, 1.1]
            readonly property real weightSum: 9.8

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: panel.tags
            boundsBehavior: Flickable.StopAtBounds
            columnSpacing: 1
            rowSpacing: 1
            columnWidthProvider: column => Math.max(panel.theme.controlHeight * 2, table.width * table.weights[column] / table.weightSum - 1)
            ScrollBar.vertical: ScrollBar {}
            onWidthChanged: table.forceLayout()

            delegate: DelegateChooser {
                role: "editor"

                DelegateChoice {
                    roleValue: "type"
                    ChoiceCell {
                        options: panel.tags.typeNames
                    }
                }
                DelegateChoice {
                    roleValue: "simulation"
                    ChoiceCell {
                        options: panel.tags.simulationNames
                    }
                }
                DelegateChoice {
                    TextCell {}
                }
            }

            Label {
                anchors.centerIn: parent
                visible: table.rows === 0
                text: panel.tags.tagCount === 0 ? qsTr("No tags yet. Add one, or import a CSV file.") : qsTr("No tag matches the search.")
                color: panel.theme.textMuted
            }
        }
    }

    // A text or number cell; the shown value follows the model unless the
    // cell is being edited.
    component TextCell: TextField {
        id: cell

        required property int row
        required property int column
        required property var display
        required property string editor

        implicitHeight: panel.theme.controlHeight
        leftPadding: panel.theme.spacing
        color: cell.editor === "value" && panel.simulation.running ? panel.theme.good : panel.theme.text
        font.bold: cell.editor === "value" && panel.simulation.running
        selectByMouse: true
        background: Rectangle {
            color: cell.row === panel.currentRow ? panel.theme.panelRaised : panel.theme.window
            border.width: cell.activeFocus ? 1 : 0
            border.color: panel.theme.accent
        }
        onActiveFocusChanged: if (cell.activeFocus)
            panel.currentRow = cell.row
        onEditingFinished: {
            panel.message = ""
            panel.tags.setCell(cell.row, cell.column, cell.text)
        }
        Keys.onEscapePressed: {
            cell.text = String(cell.display)
            cell.focus = false
        }

        Binding on text {
            when: !cell.activeFocus
            value: String(cell.display)
        }
    }

    component ChoiceCell: ComboBox {
        id: choiceCell

        required property int row
        required property int column
        required property int choice
        required property var options

        implicitHeight: panel.theme.controlHeight
        model: choiceCell.options
        currentIndex: choiceCell.choice
        focusPolicy: Qt.NoFocus
        onActivated: index => {
            panel.currentRow = choiceCell.row
            panel.tags.setCell(choiceCell.row, choiceCell.column, index)
        }
        onPressedChanged: if (choiceCell.pressed)
            panel.currentRow = choiceCell.row
    }

    Dialog {
        id: deleteDialog

        property var uses: []

        title: qsTr("Delete a tag in use?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: panel.theme.sidePanelWidth * 1.8
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: {
            panel.tags.removeTag(panel.currentRow)
            panel.currentRow = -1
        }

        Label {
            width: panel.theme.sidePanelWidth * 1.5
            wrapMode: Text.WordWrap
            text: qsTr("This tag is used %n time(s):", "", deleteDialog.uses.length) + "\n" + deleteDialog.uses.join("\n") + "\n\n" + qsTr("The bindings stay and show the tag as missing.")
        }
    }

    Dialog {
        id: usesDialog

        property var uses: []

        title: qsTr("Where the tag is used")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: panel.theme.sidePanelWidth * 1.8
        standardButtons: Dialog.Close

        Label {
            width: panel.theme.sidePanelWidth * 1.5
            wrapMode: Text.WordWrap
            text: usesDialog.uses.length > 0 ? usesDialog.uses.join("\n") : qsTr("Nothing is bound to this tag.")
        }
    }

    FileDialog {
        id: importDialog

        title: qsTr("Import tags")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("CSV files (*.csv)"), qsTr("All files (*)")]
        onAccepted: {
            panel.message = ""
            panel.tags.importCsv(importDialog.selectedFile)
        }
    }

    FileDialog {
        id: exportDialog

        title: qsTr("Export tags")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [qsTr("CSV files (*.csv)")]
        onAccepted: {
            panel.message = ""
            panel.tags.exportCsv(exportDialog.selectedFile)
        }
    }
}
