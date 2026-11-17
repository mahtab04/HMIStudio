pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Events tab: pick an event of the selected object (Click, Press,
// Release) and edit the actions it runs, in order, as in WinCC's event
// lists. Each action names a tag and a value, or a screen to open.
ColumnLayout {
    id: panel

    required property Theme theme
    required property EventModel events

    spacing: panel.theme.spacing

    Label {
        Layout.fillWidth: true
        visible: !panel.events.available
        text: qsTr("Select one object to edit what it does when the operator clicks, presses or releases it.")
        wrapMode: Text.WordWrap
        color: panel.theme.textMuted
    }

    // The event, with the number of its actions.
    RowLayout {
        Layout.fillWidth: true
        visible: panel.events.available
        spacing: 2

        Repeater {
            model: panel.events.eventNames

            ToolButton {
                id: eventButton

                required property int index
                required property string modelData

                Layout.fillWidth: true
                implicitHeight: panel.theme.controlHeight
                text: eventButton.modelData
                checked: panel.events.event === eventButton.index
                focusPolicy: Qt.NoFocus
                onClicked: panel.events.event = eventButton.index
            }
        }
    }

    ListView {
        id: list

        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: panel.events.available
        clip: true
        spacing: panel.theme.spacing
        model: panel.events
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        delegate: Frame {
            id: step

            required property int index
            required property int kind
            required property string tag
            required property string value
            required property string screen
            required property string fields

            width: list.width
            padding: panel.theme.spacing

            ColumnLayout {
                width: step.availableWidth
                spacing: panel.theme.spacing / 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: qsTr("%1.").arg(step.index + 1)
                        color: panel.theme.textMuted
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        implicitHeight: panel.theme.controlHeight
                        model: panel.events.actionNames
                        currentIndex: step.kind
                        Accessible.name: qsTr("Action %1").arg(step.index + 1)
                        onActivated: index => panel.events.setKind(step.index, index)
                    }
                    ToolButton {
                        text: "↑"
                        enabled: step.index > 0
                        implicitWidth: panel.theme.controlHeight
                        implicitHeight: panel.theme.controlHeight
                        focusPolicy: Qt.NoFocus
                        Accessible.name: qsTr("Move up")
                        onClicked: panel.events.moveAction(step.index, -1)
                    }
                    ToolButton {
                        text: "↓"
                        enabled: step.index < list.count - 1
                        implicitWidth: panel.theme.controlHeight
                        implicitHeight: panel.theme.controlHeight
                        focusPolicy: Qt.NoFocus
                        Accessible.name: qsTr("Move down")
                        onClicked: panel.events.moveAction(step.index, 1)
                    }
                    ToolButton {
                        text: "✕"
                        implicitWidth: panel.theme.controlHeight
                        implicitHeight: panel.theme.controlHeight
                        focusPolicy: Qt.NoFocus
                        Accessible.name: qsTr("Remove action")
                        onClicked: panel.events.removeAction(step.index)
                    }
                }

                // The tag of a tag step.
                RowLayout {
                    Layout.fillWidth: true
                    visible: step.fields === "tag" || step.fields === "tagValue"
                    Label {
                        Layout.preferredWidth: panel.theme.propertyLabelWidth * 0.6
                        text: qsTr("Tag")
                        color: panel.theme.textMuted
                    }
                    ComboBox {
                        id: tagBox

                        readonly property bool missing: step.tag.length > 0 && panel.events.tagNames.indexOf(step.tag) < 0

                        Layout.fillWidth: true
                        implicitHeight: panel.theme.controlHeight
                        model: panel.events.tagNames
                        currentIndex: panel.events.tagNames.indexOf(step.tag)
                        displayText: step.tag.length === 0 ? qsTr("Choose a tag") : tagBox.missing ? qsTr("%1 (missing)").arg(step.tag) : step.tag
                        Accessible.name: qsTr("Tag")
                        onActivated: index => panel.events.setTag(step.index, panel.events.tagNames[index])
                    }
                }

                // The value to set, or the step to add.
                RowLayout {
                    Layout.fillWidth: true
                    visible: step.fields === "tagValue"
                    Label {
                        Layout.preferredWidth: panel.theme.propertyLabelWidth * 0.6
                        text: step.kind === 2 ? qsTr("Step") : qsTr("Value")
                        color: panel.theme.textMuted
                    }
                    TextField {
                        id: valueField

                        Layout.fillWidth: true
                        implicitHeight: panel.theme.controlHeight
                        text: step.value
                        placeholderText: step.kind === 2 ? qsTr("5, or -5 to decrease") : qsTr("true, 0, 42.5 …")
                        Accessible.name: qsTr("Value")
                        onEditingFinished: panel.events.setValue(step.index, valueField.text)
                    }
                }

                // The screen to open.
                RowLayout {
                    Layout.fillWidth: true
                    visible: step.fields === "screen"
                    Label {
                        Layout.preferredWidth: panel.theme.propertyLabelWidth * 0.6
                        text: qsTr("Screen")
                        color: panel.theme.textMuted
                    }
                    ComboBox {
                        id: screenBox

                        readonly property bool missing: step.screen.length > 0 && panel.events.screenNames.indexOf(step.screen) < 0

                        Layout.fillWidth: true
                        implicitHeight: panel.theme.controlHeight
                        model: panel.events.screenNames
                        currentIndex: panel.events.screenNames.indexOf(step.screen)
                        displayText: step.screen.length === 0 ? qsTr("Choose a screen") : screenBox.missing ? qsTr("%1 (missing)").arg(step.screen) : step.screen
                        Accessible.name: qsTr("Screen")
                        onActivated: index => panel.events.setScreen(step.index, panel.events.screenNames[index])
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            width: list.width
            visible: list.count === 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("No actions yet.")
            color: panel.theme.textMuted
        }
    }

    Button {
        id: addButton

        Layout.fillWidth: true
        visible: panel.events.available
        implicitHeight: panel.theme.controlHeight
        text: qsTr("Add action…")
        focusPolicy: Qt.NoFocus
        onClicked: addMenu.popup(addButton, 0, addButton.height)

        Menu {
            id: addMenu

            Instantiator {
                model: panel.events.actionNames
                delegate: MenuItem {
                    required property int index
                    required property string modelData

                    text: modelData
                    onTriggered: panel.events.addAction(index)
                }
                onObjectAdded: (index, object) => addMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => addMenu.removeItem(object)
            }
        }
    }
}
