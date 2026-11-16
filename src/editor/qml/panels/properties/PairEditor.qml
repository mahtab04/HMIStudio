pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Point or size property: two number fields (x and y, or width and height).
RowLayout {
    id: row

    required property PropertyModel properties
    required property Theme theme
    required property int index
    required property string name
    required property bool bindable
    required property string binding
    required property var components
    required property bool mixed
    required property bool readOnly

    spacing: row.theme.spacing / 2

    PropertyLabel {
        theme: row.theme
        text: row.name
        properties: row.properties
        rowIndex: row.index
        bindable: row.bindable
        binding: row.binding
    }

    Repeater {
        model: 2

        TextField {
            required property int index

            Layout.fillWidth: true
            implicitHeight: row.theme.controlHeight
            text: row.mixed ? "" : row.components[index]
            placeholderText: row.mixed ? qsTr("Mixed") : ""
            readOnly: row.readOnly
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            selectByMouse: true
            Accessible.name: qsTr("%1, part %2").arg(row.name).arg(index + 1)
            onEditingFinished: row.properties.setComponent(row.index, index, text)
        }
    }
}
