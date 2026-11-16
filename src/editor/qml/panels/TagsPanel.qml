pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQml.Models

// The HMI tags editor, shown in the main area like TIA Portal's tag table:
// one row per tag, plain cells, double-click (or F2, or typing) to edit,
// "<Add new>" at the end, several tags at once, delete with a warning for
// tags in use, "where used" and CSV. While simulating, the Value column
// shows live values; typing one writes it to the tag.
Item {
    id: panel

    required property Theme theme
    required property TagModel tags
    required property SimulationController simulation

    property string message
    // The row of the current cell: a click on any cell makes it current.
    readonly property int currentRow: selection.currentIndex.valid ? selection.currentIndex.row : -1

    function selectRow(row: int) {
        if (row < 0)
            return
        const index = panel.tags.index(row, 0)
        selection.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
        table.positionViewAtRow(row, TableView.Contain)
    }

    function addTag() {
        panel.message = ""
        const row = panel.tags.addTag()
        panel.selectRow(row)
        if (row >= 0)
            table.edit(panel.tags.index(row, 0))
    }

    function deleteCurrent() {
        if (panel.currentRow < 0)
            return
        if (panel.tags.usageCount(panel.currentRow) > 0) {
            deleteDialog.uses = panel.tags.whereUsed(panel.currentRow)
            deleteDialog.open()
            return
        }
        panel.tags.removeTag(panel.currentRow)
    }

    Connections {
        target: panel.tags

        function onProblem(message: string) {
            panel.message = message
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: panel.theme.controlHeight + panel.theme.spacing
            color: panel.theme.panel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: panel.theme.padding
                anchors.rightMargin: panel.theme.padding
                spacing: panel.theme.spacing

                ToolButton {
                    text: qsTr("Add")
                    implicitHeight: panel.theme.controlHeight
                    focusPolicy: Qt.NoFocus
                    onClicked: panel.addTag()
                }
                ToolButton {
                    text: qsTr("Add several…")
                    implicitHeight: panel.theme.controlHeight
                    focusPolicy: Qt.NoFocus
                    onClicked: severalDialog.open()
                }
                ToolButton {
                    objectName: "deleteTagButton"
                    text: qsTr("Delete")
                    implicitHeight: panel.theme.controlHeight
                    enabled: panel.currentRow >= 0
                    focusPolicy: Qt.NoFocus
                    onClicked: panel.deleteCurrent()
                }
                ToolButton {
                    text: qsTr("Where used")
                    implicitHeight: panel.theme.controlHeight
                    enabled: panel.currentRow >= 0
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        usesDialog.uses = panel.tags.whereUsed(panel.currentRow)
                        usesDialog.open()
                    }
                }
                ToolSeparator {}
                ToolButton {
                    text: qsTr("Import CSV…")
                    implicitHeight: panel.theme.controlHeight
                    focusPolicy: Qt.NoFocus
                    onClicked: importDialog.open()
                }
                ToolButton {
                    text: qsTr("Export CSV…")
                    implicitHeight: panel.theme.controlHeight
                    enabled: panel.tags.tagCount > 0
                    focusPolicy: Qt.NoFocus
                    onClicked: exportDialog.open()
                }
                Label {
                    visible: panel.simulation.running
                    text: qsTr("● Live values")
                    color: panel.theme.good
                }
                TextField {
                    Layout.fillWidth: true
                    Layout.maximumWidth: panel.theme.sidePanelWidth
                    Layout.minimumWidth: panel.theme.controlHeight * 3
                    implicitHeight: panel.theme.controlHeight
                    placeholderText: qsTr("Search tags")
                    text: panel.tags.filterText
                    onTextEdited: panel.tags.filterText = text
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: panel.theme.padding
            Layout.topMargin: panel.theme.spacing / 2
            visible: panel.message.length > 0
            text: panel.message
            color: panel.theme.warning
            elide: Text.ElideRight
        }

        HorizontalHeaderView {
            id: header

            Layout.fillWidth: true
            Layout.topMargin: panel.theme.spacing / 2
            syncView: table
            clip: true
            textRole: "display"
        }

        TableView {
            id: table

            objectName: "tagTable"
            // Name, type, unit, minimum, maximum, simulation, period, comment, value.
            readonly property var weights: [1.8, 0.9, 0.7, 0.8, 0.8, 1.0, 0.8, 2.2, 1.0]
            readonly property real weightSum: 10.0

            // As tall as its rows, so "<Add new>" follows the last one.
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumHeight: Math.max(1, table.contentHeight)
            clip: true
            model: panel.tags
            boundsBehavior: Flickable.StopAtBounds
            columnSpacing: 1
            rowSpacing: 1
            selectionBehavior: TableView.SelectRows
            selectionMode: TableView.SingleSelection
            editTriggers: TableView.DoubleTapped | TableView.EditKeyPressed | TableView.AnyKeyPressed
            selectionModel: ItemSelectionModel {
                id: selection

                model: panel.tags
            }
            columnWidthProvider: column => Math.max(panel.theme.controlHeight * 2, table.width * table.weights[column] / table.weightSum - 1)
            ScrollBar.vertical: ScrollBar {}
            onWidthChanged: table.forceLayout()
            Keys.onDeletePressed: panel.deleteCurrent()

            // The grid lines are the background between the cells.
            Rectangle {
                parent: table.contentItem
                z: -1
                width: table.contentWidth
                height: table.contentHeight
                color: panel.theme.border
            }

            delegate: Rectangle {
                id: cell

                required property int row
                required property int column
                required property var display
                required property string editor
                required property int choice
                required property bool selected
                required property bool current

                readonly property bool live: cell.editor === "value" && panel.simulation.running
                readonly property bool isChoice: cell.editor === "type" || cell.editor === "simulation"

                implicitHeight: panel.theme.controlHeight
                color: cell.selected ? Qt.alpha(panel.theme.accent, 0.28) : cell.row % 2 ? panel.theme.panel : panel.theme.window
                border.width: cell.current ? 1 : 0
                border.color: panel.theme.accent

                Label {
                    anchors.fill: parent
                    visible: !cell.isChoice
                    leftPadding: panel.theme.spacing
                    rightPadding: panel.theme.spacing
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: String(cell.display ?? "")
                    color: cell.live ? panel.theme.good : panel.theme.text
                    font.bold: cell.live
                }

                // Data type and simulation: a drop-down in every row.
                ComboBox {
                    id: combo

                    anchors.fill: parent
                    anchors.margins: 1
                    visible: cell.isChoice
                    flat: true
                    model: cell.editor === "type" ? panel.tags.typeNames : cell.editor === "simulation" ? panel.tags.simulationNames : []
                    currentIndex: cell.choice
                    focusPolicy: Qt.NoFocus
                    Accessible.name: panel.tags.columnTitles[cell.column]
                    onPressedChanged: if (combo.pressed)
                        selection.setCurrentIndex(panel.tags.index(cell.row, cell.column), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
                    onActivated: index => {
                        panel.message = ""
                        panel.tags.setCell(cell.row, cell.column, index)
                    }
                }

                TableView.editDelegate: cell.isChoice ? null : textEditor

                Component {
                    id: textEditor

                    TextField {
                        id: field

                        anchors.fill: parent
                        leftPadding: panel.theme.spacing
                        text: String(cell.display ?? "")
                        verticalAlignment: TextInput.AlignVCenter
                        Component.onCompleted: field.selectAll()
                        TableView.onCommit: {
                            panel.message = ""
                            if (field.text !== String(cell.display ?? ""))
                                panel.tags.setCell(cell.row, cell.column, field.text)
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: table.rows === 0 && panel.tags.tagCount > 0
                text: qsTr("No tag matches the search.")
                color: panel.theme.textMuted
            }
        }

        // The last row adds a tag, as in TIA Portal.
        ItemDelegate {
            Layout.fillWidth: true
            implicitHeight: panel.theme.controlHeight
            leftPadding: panel.theme.spacing
            text: qsTr("<Add new>")
            font.italic: true
            palette.windowText: panel.theme.accent
            palette.buttonText: panel.theme.accent
            focusPolicy: Qt.NoFocus
            onClicked: panel.addTag()
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    // Several tags of one type at once: Name_1, Name_2, …
    Dialog {
        id: severalDialog

        title: qsTr("Add several tags")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: panel.theme.sidePanelWidth * 1.6
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            panel.message = ""
            panel.selectRow(panel.tags.addTags(baseName.text, typeBox.currentIndex, countBox.value))
        }

        GridLayout {
            width: panel.theme.sidePanelWidth * 1.4
            columns: 2
            columnSpacing: panel.theme.spacing
            rowSpacing: panel.theme.spacing

            Label {
                text: qsTr("Name")
            }
            TextField {
                id: baseName

                Layout.fillWidth: true
                implicitHeight: panel.theme.controlHeight
                text: qsTr("Tag")
            }
            Label {
                text: qsTr("Data type")
            }
            ComboBox {
                id: typeBox

                Layout.fillWidth: true
                implicitHeight: panel.theme.controlHeight
                model: panel.tags.typeNames
                currentIndex: 1
            }
            Label {
                text: qsTr("Count")
            }
            SpinBox {
                id: countBox

                Layout.fillWidth: true
                implicitHeight: panel.theme.controlHeight
                from: 1
                to: 1000
                value: 5
                editable: true
            }
            Label {
                Layout.columnSpan: 2
                text: qsTr("Creates %1_1 … %1_%2").arg(baseName.text).arg(countBox.value)
                color: panel.theme.textMuted
            }
        }
    }

    Dialog {
        id: deleteDialog

        objectName: "deleteTagDialog"
        property var uses: []

        title: qsTr("Delete a tag in use?")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: panel.theme.sidePanelWidth * 1.8
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: panel.tags.removeTag(panel.currentRow)

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
