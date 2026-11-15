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

                function onEditRejected(message: string) {
                    rejection.text = message
                }
                function onTitleChanged() {
                    rejection.text = ""
                }
            }
        }
    }
}
