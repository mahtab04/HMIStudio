pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Binds one property of the selection to a tag: a searchable tag list and
// the transform from the tag's value to the property (direct, linear
// scaling, value ranges or an expression over `x`).
Dialog {
    id: dialog

    required property Theme theme
    required property PropertyModel properties
    required property TagModel tags

    property int row: -1
    property string propertyName
    property string chosenTag
    property string message

    // Opens the editor for a row, filled from its current binding.
    function edit(row: int, name: string) {
        const info = dialog.properties.bindingInfo(row)
        const kind = info.kind ?? "direct"
        const parameters = info.parameters ?? {}
        dialog.row = row
        dialog.propertyName = name
        dialog.chosenTag = info.tag ?? ""
        dialog.message = ""
        search.text = ""
        kindBox.currentIndex = Math.max(0, dialog.properties.transformKinds.indexOf(kind))
        inMin.text = kind === "linear" ? String(parameters.inMin) : "0"
        inMax.text = kind === "linear" ? String(parameters.inMax) : "100"
        outMin.text = kind === "linear" ? String(parameters.outMin) : "0"
        outMax.text = kind === "linear" ? String(parameters.outMax) : "100"
        ranges.text = kind === "ranges" ? dialog.properties.rangesText(parameters) : ""
        expression.text = kind === "expression" ? parameters.text : ""
        dialog.open()
    }

    function apply() {
        const kind = dialog.properties.transformKinds[kindBox.currentIndex]
        let parameters = {}
        if (kind === "linear")
            parameters = {
                inMin: Number(inMin.text),
                inMax: Number(inMax.text),
                outMin: Number(outMin.text),
                outMax: Number(outMax.text)
            }
        else if (kind === "ranges")
            parameters = dialog.properties.rangesParameters(ranges.text)
        else if (kind === "expression")
            parameters = {
                text: expression.text
            }
        if (kind === "ranges" && parameters.ranges === undefined)
            return
        if (dialog.properties.setBinding(dialog.row, dialog.chosenTag, kind, parameters))
            dialog.close()
    }

    title: qsTr("Bind %1 to a tag").arg(dialog.propertyName)
    width: dialog.theme.sidePanelWidth * 1.7
    modal: true
    anchors.centerIn: Overlay.overlay

    Connections {
        target: dialog.properties
        enabled: dialog.visible

        function onEditRejected(message: string) {
            dialog.message = message
        }
    }

    ColumnLayout {
        width: dialog.theme.sidePanelWidth * 1.5
        spacing: dialog.theme.spacing

        TextField {
            id: search

            Layout.fillWidth: true
            implicitHeight: dialog.theme.controlHeight
            placeholderText: qsTr("Search tags")
        }

        ListView {
            id: tagList

            Layout.fillWidth: true
            Layout.preferredHeight: dialog.theme.controlHeight * 5
            clip: true
            model: dialog.tags.namesMatching(search.text)
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                required property string modelData

                width: tagList.width
                implicitHeight: dialog.theme.controlHeight
                text: modelData
                highlighted: modelData === dialog.chosenTag
                onClicked: dialog.chosenTag = modelData
            }

            Label {
                anchors.centerIn: parent
                visible: tagList.count === 0
                text: dialog.tags.tagCount === 0 ? qsTr("No tags yet: add them in the Tags panel.") : qsTr("No tag matches.")
                color: dialog.theme.textMuted
            }
        }

        Label {
            Layout.fillWidth: true
            text: dialog.chosenTag.length > 0 ? qsTr("Tag: %1").arg(dialog.chosenTag) : qsTr("Pick a tag.")
            color: dialog.theme.text
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: dialog.theme.spacing

            Label {
                text: qsTr("Transform")
                color: dialog.theme.textMuted
            }
            ComboBox {
                id: kindBox

                Layout.fillWidth: true
                implicitHeight: dialog.theme.controlHeight
                model: dialog.properties.transformNames
            }
        }

        StackLayout {
            Layout.fillWidth: true
            currentIndex: kindBox.currentIndex

            Label {
                text: qsTr("The tag's value is used as it is.")
                color: dialog.theme.textMuted
                wrapMode: Text.WordWrap
            }

            GridLayout {
                columns: 4
                columnSpacing: dialog.theme.spacing

                Label {
                    text: qsTr("Tag from")
                    color: dialog.theme.textMuted
                }
                TextField {
                    id: inMin

                    Layout.fillWidth: true
                    implicitHeight: dialog.theme.controlHeight
                }
                Label {
                    text: qsTr("to")
                    color: dialog.theme.textMuted
                }
                TextField {
                    id: inMax

                    Layout.fillWidth: true
                    implicitHeight: dialog.theme.controlHeight
                }
                Label {
                    text: qsTr("Property from")
                    color: dialog.theme.textMuted
                }
                TextField {
                    id: outMin

                    Layout.fillWidth: true
                    implicitHeight: dialog.theme.controlHeight
                }
                Label {
                    text: qsTr("to")
                    color: dialog.theme.textMuted
                }
                TextField {
                    id: outMax

                    Layout.fillWidth: true
                    implicitHeight: dialog.theme.controlHeight
                }
            }

            TextArea {
                id: ranges

                placeholderText: qsTr("0..50 = #2E9B4F\n50..80 = #F0B400\nelse = #D64541")
                wrapMode: TextEdit.NoWrap
            }

            TextField {
                id: expression

                implicitHeight: dialog.theme.controlHeight
                placeholderText: qsTr("x * 1.8 + 32   (x is the tag's value)")
            }
        }

        Label {
            Layout.fillWidth: true
            visible: dialog.message.length > 0
            text: dialog.message
            wrapMode: Text.WordWrap
            color: dialog.theme.warning
            font: dialog.theme.fontSmall
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Bind")
            enabled: dialog.chosenTag.length > 0
            onClicked: dialog.apply()
        }
        Button {
            text: qsTr("Remove binding")
            enabled: dialog.properties.bindingInfo(dialog.row).tag !== undefined
            onClicked: {
                dialog.properties.removeBinding(dialog.row)
                dialog.close()
            }
        }
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
