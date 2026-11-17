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

    readonly property string message: panel.tags.message
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
        panel.tags.clearMessage()
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

        function onTagCreated(row: int) {
            panel.selectRow(row)
        }
        function onViewChanged() {
            table.forceLayout()
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
                    onClicked: newTagDialog.open()
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
                    text: panel.simulation.monitoring ? qsTr("PLC monitoring - read only") : qsTr("Simulated values")
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

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: panel.theme.padding
            Layout.rightMargin: panel.theme.padding
            spacing: panel.theme.spacing

            ComboBox {
                implicitHeight: panel.theme.controlHeight
                objectName: "tagViewPicker"
                model: [qsTr("Tags"), qsTr("Simulation settings")]
                currentIndex: panel.tags.simulationSettings ? 1 : 0
                Accessible.name: qsTr("Tag view")
                onActivated: index => panel.tags.simulationSettings = index === 1
            }
            ComboBox {
                implicitHeight: panel.theme.controlHeight
                objectName: "tagTablePicker"
                model: panel.tags.tableNames
                currentIndex: panel.tags.tableFilter.length === 0 ? 0 : panel.tags.tableNames.indexOf(panel.tags.tableFilter)
                Accessible.name: qsTr("Filter tag table")
                onActivated: index => panel.tags.tableFilter = index === 0 ? "" : panel.tags.tableNames[index]
            }
            Button {
                implicitHeight: panel.theme.controlHeight
                objectName: "connectionsButton"
                text: qsTr("Connections")
                onClicked: connectionsDialog.open()
            }
            Item { Layout.fillWidth: true }
            Button {
                implicitHeight: panel.theme.controlHeight
                objectName: "monitorPlcsButton"
                text: panel.simulation.running && panel.simulation.monitoring ? qsTr("Stop monitoring") : qsTr("Monitor PLCs")
                onClicked: panel.simulation.running && panel.simulation.monitoring ? panel.simulation.stop() : panel.simulation.startMonitoring()
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: panel.theme.padding
            Layout.topMargin: panel.theme.spacing / 2
            visible: panel.message.length > 0 || panel.tags.monitorProblem.length > 0
            text: panel.message.length > 0 ? panel.message : panel.tags.monitorProblem
            color: panel.theme.warning
            wrapMode: Text.WordWrap
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

            // As tall as its rows, so "<Add new>" follows the last one.
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: table.contentHeight
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
            columnWidthProvider: column => panel.tags.columnWidth(column, table.width, panel.theme.controlHeight * 2)
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}
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
                required property string toolTip
                required property bool selected
                required property bool current

                readonly property bool live: cell.editor === "value" && panel.simulation.running
                readonly property bool isChoice: cell.editor === "type" || cell.editor === "simulation" || cell.editor === "connection" || cell.editor === "limit"

                implicitHeight: panel.theme.controlHeight
                color: cell.selected ? Qt.alpha(panel.theme.accent, 0.28) : cell.row % 2 ? panel.theme.panel : panel.theme.window
                border.width: cell.current ? 1 : 0
                border.color: panel.theme.accent
                Accessible.description: cell.toolTip
                ToolTip.visible: hover.hovered && cell.toolTip.length > 0
                ToolTip.text: cell.toolTip
                HoverHandler { id: hover }

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
                    model: cell.editor === "type" ? panel.tags.typeNames : cell.editor === "simulation" ? panel.tags.simulationNames : cell.editor === "connection" ? panel.tags.connections.names : cell.editor === "limit" ? panel.tags.limitNames : []
                    currentIndex: cell.choice
                    displayText: cell.choice < 0 ? String(cell.display) : combo.currentText
                    focusPolicy: Qt.NoFocus
                    Accessible.name: panel.tags.columnTitles[cell.column]
                    onPressedChanged: if (combo.pressed)
                        selection.setCurrentIndex(panel.tags.index(cell.row, cell.column), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
                    onActivated: index => {
                        panel.tags.clearMessage()
                        panel.tags.setCell(cell.row, cell.column, index)
                    }
                }

                TableView.editDelegate: cell.isChoice || cell.editor === "readonly" ? null : textEditor

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
                            panel.tags.clearMessage()
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
            onClicked: newTagDialog.open()
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    NewTagDialog {
        id: newTagDialog
        theme: panel.theme
        tags: panel.tags
    }

    ConnectionsDialog {
        id: connectionsDialog
        theme: panel.theme
        connections: panel.tags.connections
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
            panel.tags.clearMessage()
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
            panel.tags.clearMessage()
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
            panel.tags.clearMessage()
            panel.tags.exportCsv(exportDialog.selectedFile)
        }
    }
}
