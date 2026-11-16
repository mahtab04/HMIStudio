pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root

    required property EditorController editor
    required property Theme theme
    required property ToolboxModel toolbox
    required property ToolboxDragController toolboxDrag
    required property DocumentController document
    required property AppSettings settings
    required property PropertyModel properties
    required property LayerModel layers
    required property ScreenListModel screens
    required property TagModel tags
    required property SimulationController simulation

    width: 1440
    height: 900
    visible: true
    title: qsTr("%1 — HMI Studio").arg(root.document.title)
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
        simulation: root.simulation
        theme: root.theme
        document: root.document
        settings: root.settings
    }

    footer: StatusBar {
        editor: root.editor
        theme: root.theme
        document: root.document
    }

    // Unsaved changes are asked about first; the controller approves the
    // close once they are saved or discarded.
    onClosing: close => close.accepted = root.document.requestClose()

    Connections {
        target: root.document

        function onCloseApproved() {
            root.close()
        }
    }

    // Panel sizes are restored at start and saved when the window goes.
    SplitView {
        id: mainSplit

        anchors.fill: parent
        orientation: Qt.Horizontal

        Component.onCompleted: mainSplit.restoreState(root.settings.mainSplitState)
        Component.onDestruction: root.settings.mainSplitState = mainSplit.saveState()

        ToolboxPanel {
            visible: root.settings.toolboxPanelVisible
            SplitView.preferredWidth: root.theme.sidePanelWidth
            SplitView.minimumWidth: root.theme.toolboxTile * 2
            editor: root.editor
            theme: root.theme
            toolbox: root.toolbox
            toolboxDrag: root.toolboxDrag
            layers: root.layers
        }

        SplitView {
            id: centreSplit

            SplitView.fillWidth: true
            orientation: Qt.Vertical

            Component.onCompleted: centreSplit.restoreState(root.settings.centreSplitState)
            Component.onDestruction: root.settings.centreSplitState = centreSplit.saveState()

            CanvasPanel {
                objectName: "workArea"
                SplitView.fillHeight: true
                editor: root.editor
                theme: root.theme
                screens: root.screens
                tags: root.tags
                simulation: root.simulation
            }

            BottomPanel {
                visible: root.settings.bottomPanelVisible
                SplitView.preferredHeight: root.theme.bottomPanelHeight
                SplitView.minimumHeight: root.theme.controlHeight
                theme: root.theme
                simulation: root.simulation
            }
        }

        PropertiesPanel {
            visible: root.settings.propertiesPanelVisible
            SplitView.preferredWidth: root.theme.sidePanelWidth
            SplitView.minimumWidth: root.theme.toolboxTile * 2
            editor: root.editor
            theme: root.theme
            properties: root.properties
            tags: root.tags
        }
    }

    DocumentDialogs {
        document: root.document
        theme: root.theme
    }

    Shortcut {
        sequence: "F5"
        onActivated: root.simulation.running ? root.simulation.restart() : root.simulation.start()
    }
    Shortcut {
        sequence: "Shift+F5"
        onActivated: root.simulation.stop()
    }

    // The runtime simulator opens in its own window while it runs.
    Loader {
        active: root.simulation.running
        sourceComponent: SimulatorWindow {
            simulation: root.simulation
            theme: root.theme
        }
    }
    Shortcut {
        sequences: [StandardKey.New]
        onActivated: root.document.newProject()
    }
    Shortcut {
        sequences: [StandardKey.Open]
        onActivated: root.document.open()
    }
    Shortcut {
        sequences: [StandardKey.Save]
        onActivated: root.document.save()
    }
    Shortcut {
        sequences: [StandardKey.SaveAs, "Ctrl+Shift+S"]
        onActivated: root.document.chooseSaveLocation()
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
        sequences: [StandardKey.Copy]
        onActivated: root.editor.copy()
    }
    Shortcut {
        sequences: [StandardKey.Cut]
        onActivated: root.editor.cut()
    }
    Shortcut {
        sequences: [StandardKey.Paste]
        onActivated: root.editor.paste()
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: root.editor.duplicate()
    }
    Shortcut {
        sequences: [StandardKey.SelectAll]
        onActivated: root.editor.selectAll()
    }
    Shortcut {
        sequence: "Ctrl+G"
        onActivated: root.editor.group()
    }
    Shortcut {
        sequence: "Ctrl+Shift+G"
        onActivated: root.editor.ungroup()
    }
    Shortcut {
        sequence: "Ctrl+]"
        onActivated: root.editor.reorder(EditorController.BringForward)
    }
    Shortcut {
        sequence: "Ctrl+["
        onActivated: root.editor.reorder(EditorController.SendBackward)
    }
    Shortcut {
        sequence: "Ctrl+Shift+]"
        onActivated: root.editor.reorder(EditorController.BringToFront)
    }
    Shortcut {
        sequence: "Ctrl+Shift+["
        onActivated: root.editor.reorder(EditorController.SendToBack)
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
