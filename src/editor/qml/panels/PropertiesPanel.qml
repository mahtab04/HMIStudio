pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

// Right panel: the properties of the selection, or of the screen when
// nothing is selected, grouped by category. The editor of each row is
// chosen by its value type.
Pane {
    id: panel

    required property EditorController editor
    required property Theme theme
    required property PropertyModel properties
    required property TagModel tags

    padding: panel.theme.padding

    ColumnLayout {
        anchors.fill: parent
        spacing: panel.theme.spacing

        Label {
            text: qsTr("PROPERTIES")
            font: panel.theme.fontHeading
            color: panel.theme.textMuted
        }
        Label {
            Layout.fillWidth: true
            text: panel.properties.title
            elide: Text.ElideRight
            color: panel.theme.text
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: panel.theme.spacing / 2
            model: panel.properties
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            section.property: "category"
            section.delegate: Label {
                required property string section

                width: list.width
                topPadding: panel.theme.spacing
                text: section.toUpperCase()
                font: panel.theme.fontSmall
                color: panel.theme.textMuted
            }

            delegate: DelegateChooser {
                role: "editor"

                DelegateChoice {
                    roleValue: "screen"
                    ScreenPickerEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "tagReference"
                    TagPickerEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                        transformAllowed: false
                    }
                }
                DelegateChoice {
                    roleValue: "tag"
                    TagPickerEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "number"
                    NumberEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "integer"
                    NumberEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "text"
                    TextEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "asset"
                    AssetEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "points"
                    TextEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "color"
                    ColorEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "font"
                    FontEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "enum"
                    EnumEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "bool"
                    BoolEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "point"
                    PairEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
                DelegateChoice {
                    roleValue: "size"
                    PairEditor {
                        width: list.width
                        properties: panel.properties
                        theme: panel.theme
                    }
                }
            }
        }

        // Why the last edit was refused, until the next one.
        Label {
            id: rejection

            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.WordWrap
            color: panel.theme.warning
            font: panel.theme.fontSmall

            Connections {
                target: panel.properties

                function onSvgImportQuestion(row: int, file: url, elements: string) {
                    svgDialog.row = row
                    svgDialog.file = file
                    svgDialog.elements = elements
                    svgDialog.open()
                }

                function onEditRejected(message: string) {
                    if (!bindingPopup.visible)
                        rejection.text = message
                }
                function onBindingRequested(row: int, name: string) {
                    bindingPopup.edit(row, name)
                }
                function onTitleChanged() {
                    rejection.text = ""
                }
            }
        }
    }

    BindingPopup {
        id: bindingPopup

        theme: panel.theme
        properties: panel.properties
        tags: panel.tags
    }

    // An imported SVG has parts the renderer will not draw: import it
    // anyway, or as a picture of how it looks now.
    Dialog {
        id: svgDialog

        property int row: -1
        property url file
        property string elements

        title: qsTr("SVG content not supported")
        width: panel.theme.sidePanelWidth * 1.6
        modal: true
        anchors.centerIn: Overlay.overlay
        footer: DialogButtonBox {
            Button {
                text: qsTr("Import as is")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: panel.properties.importAsset(svgDialog.row, svgDialog.file, PropertyModel.ImportAsIs)
            }
            Button {
                text: qsTr("Rasterize")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: panel.properties.importAsset(svgDialog.row, svgDialog.file, PropertyModel.Rasterize)
            }
            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }

        Label {
            width: panel.theme.sidePanelWidth * 1.3
            wrapMode: Text.WordWrap
            text: qsTr("This file uses %1, which will not be drawn. Import it as it is, or rasterize it to a picture?").arg(svgDialog.elements)
        }
    }
}
