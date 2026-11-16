import QtQuick
import QtQuick.Controls

// One toolbox entry. Tap selects the create tool for this type; dragging the
// tile onto the canvas places an object where it is dropped.
ToolButton {
    id: tile

    required property string key
    required property string name
    required property url iconSource
    required property EditorController editor
    required property Theme theme
    required property ToolboxDragController toolboxDrag

    text: tile.name
    display: AbstractButton.TextUnderIcon
    icon.source: tile.iconSource
    // Toolbox icons are larger than toolbar icons, so their detail shows.
    icon.width: Math.round(tile.theme.iconSize * 1.5)
    icon.height: Math.round(tile.theme.iconSize * 1.5)
    // Toolbox icons are drawn in their own colours, not tinted.
    icon.color: "transparent"
    font: tile.theme.fontSmall
    checked: tile.editor.createType === tile.key
    focusPolicy: Qt.NoFocus
    Accessible.name: qsTr("Place %1").arg(tile.name)

    onClicked: tile.editor.createTool(tile.key)

    DragHandler {
        id: dragHandler

        target: null
        onActiveChanged: tile.toolboxDrag.setActive(dragHandler.active, tile.key)
        onCentroidChanged: tile.toolboxDrag.move(dragHandler.centroid.scenePosition)
    }
}
