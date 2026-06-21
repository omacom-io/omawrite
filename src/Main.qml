import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    width: 1280
    height: 820
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: (backend.modified ? "* " : "") + backend.fileName + " - Omawrite"

    readonly property bool darkMode: backend.darkMode
    readonly property color pageColor: darkMode ? "#101010" : "#fbfaf5"
    readonly property color textColor: darkMode ? "#aaa9a6" : "#232323"
    readonly property color strongTextColor: darkMode ? "#d2d2d0" : "#161616"
    readonly property color mutedColor: darkMode ? "#5b5d62" : "#aaa49d"
    readonly property color selectionFill: darkMode ? "#3d4b5e" : "#c7d7ee"
    readonly property int editorWidth: Math.min(640, Math.max(360, width - 120))
    property bool previewMode: false

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: darkMode ? "#d9d4c8" : "#202124"
    color: pageColor

    function toggleFullScreen() {
        win.visibility = win.visibility === Window.FullScreen
            ? Window.Windowed
            : Window.FullScreen;
    }

    function syncEditorFromBackend() {
        if (editor.text !== backend.documentText)
            editor.text = backend.documentText;
    }

    Shortcut {
        sequence: "Ctrl+S"
        context: Qt.ApplicationShortcut
        onActivated: backend.save()
    }

    Shortcut {
        sequence: "Ctrl+O"
        context: Qt.ApplicationShortcut
        onActivated: backend.openDialog()
    }

    Shortcut {
        sequence: "Ctrl+N"
        context: Qt.ApplicationShortcut
        onActivated: backend.newWindow()
    }

    Shortcut {
        sequence: "Ctrl+Shift+S"
        context: Qt.ApplicationShortcut
        onActivated: backend.saveAsDialog()
    }

    Shortcut {
        sequence: "Meta+F"
        context: Qt.ApplicationShortcut
        onActivated: toggleFullScreen()
    }

    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        onActivated: toggleFullScreen()
    }

    Shortcut {
        sequence: "Ctrl+Z"
        context: Qt.WindowShortcut
        onActivated: editor.undo()
    }

    Shortcut {
        sequence: "Ctrl+Shift+Z"
        context: Qt.WindowShortcut
        onActivated: editor.redo()
    }

    Shortcut {
        sequence: "Ctrl+Y"
        context: Qt.WindowShortcut
        onActivated: editor.redo()
    }

    Shortcut {
        sequence: "Ctrl+E"
        context: Qt.ApplicationShortcut
        onActivated: previewMode = !previewMode
    }

    Connections {
        target: backend

        function onDocumentTextChanged() {
            win.syncEditorFromBackend();
        }
    }

    Item {
        anchors.fill: parent

        Text {
            anchors.centerIn: parent
            text: "OMAWRITE"
            color: darkMode ? "#ffffff" : "#000000"
            opacity: darkMode ? 0.025 : 0.035
            font.family: "iA Writer Mono S"
            font.pixelSize: Math.max(36, Math.min(72, win.width / 15))
            font.bold: true
            visible: !previewMode
        }

        Flickable {
            id: editorFlick
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 0
            anchors.bottomMargin: 0
            clip: true
            contentWidth: width
            contentHeight: Math.max(height, editor.y + editor.implicitHeight + 220)
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                active: hovered || pressed
            }

            TextArea {
                id: editor
                x: Math.round((editorFlick.width - width) / 2)
                y: Math.max(42, Math.round(win.height * 0.05))
                width: win.editorWidth
                height: Math.max(editorFlick.height - y - 96, implicitHeight + 20)
                visible: !previewMode
                text: ""
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                persistentSelection: true
                activeFocusOnPress: true
                placeholderText: "# Start writing"
                placeholderTextColor: win.mutedColor
                color: win.textColor
                selectedTextColor: win.strongTextColor
                selectionColor: win.selectionFill
                font.family: "iA Writer Mono S"
                font.pixelSize: 16
                font.weight: Font.Medium
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                background: null
                cursorDelegate: Rectangle {
                    width: 1
                    color: win.strongTextColor
                }

                onTextChanged: {
                    if (backend.documentText !== text)
                        backend.documentText = text;
                }

                Component.onCompleted: {
                    win.syncEditorFromBackend();
                    backend.attachDocument(textDocument);
                    forceActiveFocus();
                }
            }

            TextEdit {
                id: preview
                x: Math.round((editorFlick.width - width) / 2)
                y: Math.max(42, Math.round(win.height * 0.05))
                width: win.editorWidth
                visible: previewMode
                readOnly: true
                text: backend.documentText
                textFormat: TextEdit.MarkdownText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                color: win.textColor
                selectionColor: win.selectionFill
                selectedTextColor: win.strongTextColor
                font.family: "iA Writer Mono S"
                font.pixelSize: 16
            }
        }

        Row {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.bottomMargin: 10
            spacing: 12
            opacity: 0.55

            Label {
                text: previewMode ? "A" : "#"
                color: win.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: 12
                font.bold: true

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: previewMode = !previewMode
                }
            }

            Label {
                text: backend.modified ? "unsaved" : backend.status
                color: win.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: 11
                visible: text !== ""
                elide: Text.ElideRight
                width: Math.min(360, win.width / 3)
            }
        }

        Label {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 12
            anchors.bottomMargin: 10
            text: backend.wordCount + (backend.wordCount === 1 ? " Word" : " Words")
            color: win.mutedColor
            opacity: 0.75
            font.family: "iA Writer Mono S"
            font.pixelSize: 11
        }
    }

}
