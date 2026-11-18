pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

// The HMI alarms editor, shown in the work area like TIA Portal's: Discrete
// alarms, Analog alarms, Alarm classes and Runtime settings, each a table with plain cells
// (double-click or F2 to edit), drop-downs for choices, colour swatches for
// the class colours, "<Add new>" after the last row and Delete.
Item {
    id: panel

    required property Theme theme
    required property AlarmModel alarms

    property string message
    readonly property int currentRow: selection.currentIndex.valid ? selection.currentIndex.row : -1
    readonly property bool settingsPage: panel.alarms.page === AlarmModel.SettingsPage

    function addRow() {
        panel.message = ""
        const row = panel.alarms.addRow()
        if (row < 0)
            return
        selection.setCurrentIndex(panel.alarms.index(row, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
        table.positionViewAtRow(row, TableView.Contain)
    }

    Connections {
        target: panel.alarms

        function onProblem(message: string) {
            panel.message = message
        }
        function onPageChanged() {
            selection.clear()
            table.forceLayout()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: panel.theme.controlHeight + panel.theme.spacing
            color: panel.theme.panel

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: panel.theme.padding
                anchors.rightMargin: panel.theme.padding
                spacing: 2

                // Discrete alarms | Analog alarms | Alarm classes
                Repeater {
                    model: panel.alarms.pageNames

                    ToolButton {
                        id: pageButton

                        required property int index
                        required property string modelData

                        objectName: "alarmPage" + pageButton.index
                        text: pageButton.modelData
                        checked: panel.alarms.page === pageButton.index
                        implicitHeight: panel.theme.controlHeight
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            panel.message = ""
                            panel.alarms.page = pageButton.index
                        }
                    }
                }
                ToolSeparator {}
                ToolButton {
                    text: qsTr("Add")
                    implicitHeight: panel.theme.controlHeight
                    enabled: !panel.settingsPage
                    focusPolicy: Qt.NoFocus
                    onClicked: panel.addRow()
                }
                ToolButton {
                    objectName: "deleteAlarmButton"
                    text: qsTr("Delete")
                    implicitHeight: panel.theme.controlHeight
                    enabled: panel.currentRow >= 0 && !panel.settingsPage
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        panel.message = ""
                        panel.alarms.removeRow(panel.currentRow)
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("Alarm texts show values: {1} an alarm parameter, {TagName} a tag, {1:6.2} width and decimals")
                    color: panel.theme.textMuted
                    elide: Text.ElideRight
                    Layout.maximumWidth: panel.width * 0.5
                    visible: panel.alarms.page < AlarmModel.ClassesPage
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
            wrapMode: Text.WordWrap
        }

        HorizontalHeaderView {
            Layout.fillWidth: true
            Layout.topMargin: panel.theme.spacing / 2
            syncView: table
            clip: true
            textRole: "display"
        }

        TableView {
            id: table

            objectName: "alarmTable"
            // As tall as its rows, so "<Add new>" follows the last one.
            Layout.fillWidth: true
            Layout.fillHeight: true
            // All its rows, as far as they fit; the spacer below takes the rest.
            Layout.preferredHeight: Math.max(1, table.rows * (panel.theme.controlHeight + table.rowSpacing))
            Layout.maximumHeight: table.Layout.preferredHeight
            clip: true
            model: panel.alarms
            boundsBehavior: Flickable.StopAtBounds
            columnSpacing: 1
            rowSpacing: 1
            selectionBehavior: TableView.SelectRows
            selectionMode: TableView.SingleSelection
            editTriggers: TableView.DoubleTapped | TableView.EditKeyPressed | TableView.AnyKeyPressed
            selectionModel: ItemSelectionModel {
                id: selection

                model: panel.alarms
            }
            columnWidthProvider: column => panel.alarms.columnWidth(column, table.width, panel.theme.controlHeight * 1.5)
            ScrollBar.vertical: ScrollBar {}
            onWidthChanged: table.forceLayout()

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
                required property color swatch
                required property bool selected
                required property bool current

                readonly property bool isChoice: ["class", "tag", "edge", "mode", "stateMachine", "flag"].indexOf(cell.editor) >= 0
                readonly property var choices: cell.editor === "class" ? panel.alarms.classNames : cell.editor === "tag" ? panel.alarms.tagNames : cell.editor === "edge" ? panel.alarms.edgeNames : cell.editor === "mode" ? panel.alarms.modeNames : cell.editor === "stateMachine" ? panel.alarms.stateMachineNames : cell.editor === "flag" ? panel.alarms.yesNo : []

                implicitHeight: panel.theme.controlHeight
                color: cell.selected ? Qt.alpha(panel.theme.accent, 0.28) : cell.row % 2 ? panel.theme.panel : panel.theme.window
                border.width: cell.current ? 1 : 0
                border.color: panel.theme.accent

                // A colour: its swatch and code.
                Rectangle {
                    visible: cell.editor === "color" && cell.swatch.a > 0
                    x: panel.theme.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    width: panel.theme.controlHeight * 0.55
                    height: width
                    radius: 2
                    color: cell.swatch
                    border.color: panel.theme.border
                }
                Label {
                    anchors.fill: parent
                    visible: !cell.isChoice
                    leftPadding: panel.theme.spacing + (cell.editor === "color" ? panel.theme.controlHeight * 0.7 : 0)
                    rightPadding: panel.theme.spacing
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: String(cell.display ?? "")
                    color: cell.editor === "readonly" ? panel.theme.textMuted : panel.theme.text
                }

                ComboBox {
                    id: combo

                    anchors.fill: parent
                    anchors.margins: 1
                    visible: cell.isChoice
                    flat: true
                    model: cell.choices
                    currentIndex: cell.choice
                    displayText: cell.choice < 0 ? String(cell.display ?? "") : combo.currentText
                    focusPolicy: Qt.NoFocus
                    Accessible.name: panel.alarms.columnTitles[cell.column] ?? ""
                    onPressedChanged: if (combo.pressed)
                        selection.setCurrentIndex(panel.alarms.index(cell.row, cell.column), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
                    onActivated: index => {
                        panel.message = ""
                        panel.alarms.setCell(cell.row, cell.column, index)
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
                        placeholderText: cell.editor === "color" ? "#RRGGBB" : ""
                        verticalAlignment: TextInput.AlignVCenter
                        Component.onCompleted: field.selectAll()
                        TableView.onCommit: {
                            panel.message = ""
                            if (field.text !== String(cell.display ?? ""))
                                panel.alarms.setCell(cell.row, cell.column, field.text)
                        }
                    }
                }
            }
        }

        // The last row adds an alarm (or a class), as in TIA Portal.
        ItemDelegate {
            objectName: "addAlarmRow"
            visible: !panel.settingsPage
            Layout.fillWidth: true
            implicitHeight: panel.theme.controlHeight
            leftPadding: panel.theme.spacing
            text: qsTr("<Add new>")
            font.italic: true
            palette.windowText: panel.theme.accent
            palette.buttonText: panel.theme.accent
            focusPolicy: Qt.NoFocus
            onClicked: panel.addRow()
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 0
        }
    }
}
