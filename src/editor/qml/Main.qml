pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root

    required property EditorController editor
    required property Theme theme
    required property ToolboxModel toolbox
    required property ToolboxDragController toolboxDrag

    width: 1440
    height: 900
    visible: true
    title: qsTr("%1 — HMI Studio").arg(root.editor.projectTitle)
    color: root.theme.window
    font: root.theme.font

    palette.window: root.theme.panel
    palette.windowText: root.theme.text
    palette.base: root.theme.window
    palette.alternateBase: root.theme.panelRaised
    palette.text: root.theme.text
    palette.placeholderText: root.theme.textMuted
    palette.button: root.theme.panelRaised
    palette.buttonText: root.theme.text
    palette.brightText: root.theme.text
    palette.highlight: root.theme.accent
    palette.highlightedText: root.theme.accentText
    palette.toolTipBase: root.theme.panelRaised
    palette.toolTipText: root.theme.text
    palette.light: root.theme.panelRaised
    palette.midlight: root.theme.border
    palette.mid: root.theme.border
    palette.dark: root.theme.window
    palette.shadow: root.theme.window
    palette.disabled.text: root.theme.textMuted
    palette.disabled.buttonText: root.theme.textMuted
    palette.disabled.windowText: root.theme.textMuted

    header: TopBar {
        editor: root.editor
        theme: root.theme
    }

    footer: StatusBar {
        editor: root.editor
        theme: root.theme
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        ToolboxPanel {
            SplitView.preferredWidth: root.theme.sidePanelWidth
            SplitView.minimumWidth: root.theme.toolboxTile * 2
            editor: root.editor
            theme: root.theme
            toolbox: root.toolbox
            toolboxDrag: root.toolboxDrag
        }

        SplitView {
            SplitView.fillWidth: true
            orientation: Qt.Vertical

            CanvasPanel {
                SplitView.fillHeight: true
                editor: root.editor
                theme: root.theme
            }

            BottomPanel {
                SplitView.preferredHeight: root.theme.bottomPanelHeight
                SplitView.minimumHeight: root.theme.controlHeight
                theme: root.theme
            }
        }

        PropertiesPanel {
            SplitView.preferredWidth: root.theme.sidePanelWidth
            SplitView.minimumWidth: root.theme.toolboxTile * 2
            editor: root.editor
            theme: root.theme
        }
    }

    Shortcut {
        sequences: [StandardKey.Undo]
        onActivated: root.editor.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        onActivated: root.editor.redo()
    }
    Shortcut {
        sequences: [StandardKey.ZoomIn, "Ctrl+="]
        onActivated: root.editor.zoomIn()
    }
    Shortcut {
        sequences: [StandardKey.ZoomOut]
        onActivated: root.editor.zoomOut()
    }
    Shortcut {
        sequence: "Ctrl+0"
        onActivated: root.editor.zoomToFit()
    }
    Shortcut {
        sequence: "Ctrl+1"
        onActivated: root.editor.zoomToActualSize()
    }
    Shortcut {
        sequence: "V"
        onActivated: root.editor.selectTool()
    }
    Shortcut {
        sequence: "H"
        onActivated: root.editor.panTool()
    }
}
