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
    readonly property color textColor: darkMode ? "#b7b7b3" : "#333331"
    readonly property color strongTextColor: darkMode ? "#d0d0cc" : "#161616"
    readonly property color mutedColor: darkMode ? "#676862" : "#aaa49d"
    readonly property color selectionFill: darkMode ? "#4a4a46" : "#c7d7ee"
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

            TextEdit {
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
                color: win.textColor
                selectedTextColor: win.strongTextColor
                selectionColor: win.selectionFill
                font.family: "iA Writer Mono S"
                font.pixelSize: 18
                font.weight: Font.Normal
                renderType: TextEdit.NativeRendering
                cursorDelegate: Rectangle {
                    width: 1
                    color: win.strongTextColor
                }

                function replaceSelectionWith(replacement) {
                    var start = Math.min(selectionStart, selectionEnd);
                    var end = Math.max(selectionStart, selectionEnd);
                    if (start !== end) {
                        remove(start, end);
                        cursorPosition = start;
                    }
                    insert(cursorPosition, replacement);
                    cursorPosition += replacement.length;
                }

                function escapeMarkdownLinkText(linkText) {
                    return linkText.replace(/\\/g, "\\\\")
                                   .replace(/\[/g, "\\[")
                                   .replace(/\]/g, "\\]");
                }

                function escapeMarkdownLinkDestination(linkUrl) {
                    return linkUrl.replace(/\\/g, "\\\\")
                                  .replace(/\(/g, "\\(")
                                  .replace(/\)/g, "\\)");
                }

                function pasteClipboardUrlAsMarkdownLink() {
                    var start = Math.min(selectionStart, selectionEnd);
                    var end = Math.max(selectionStart, selectionEnd);
                    if (start === end)
                        return false;

                    var url = backend.clipboardUrl();
                    if (url === "")
                        return false;

                    var selected = text.slice(start, end);
                    var leading = selected.match(/^\s*/)[0];
                    var trailing = selected.match(/\s*$/)[0];
                    var linkText = selected.slice(leading.length,
                                                  selected.length - trailing.length);
                    if (linkText === "")
                        return false;

                    replaceSelectionWith(leading + "[" + escapeMarkdownLinkText(linkText) + "]("
                                         + escapeMarkdownLinkDestination(url) + ")" + trailing);
                    return true;
                }

                function lineBounds(position) {
                    var start = text.lastIndexOf("\n", Math.max(0, position - 1)) + 1;
                    var end = text.indexOf("\n", position);
                    if (end === -1)
                        end = text.length;
                    return { start: start, end: end };
                }

                function addRange(ranges, start, end) {
                    if (end > start)
                        ranges.push({ start: start, end: end });
                }

                function hiddenRangesForLine(lineText, lineStart) {
                    var ranges = [];
                    var match;
                    var re = /(\*\*|__)(.+?)(\1)/g;
                    while ((match = re.exec(lineText)) !== null) {
                        addRange(ranges, lineStart + match.index,
                                 lineStart + match.index + match[1].length);
                        addRange(ranges,
                                 lineStart + match.index + match[0].length - match[3].length,
                                 lineStart + match.index + match[0].length);
                    }

                    re = /(^|[^*])\*([^*\n]+)\*(?!\*)/g;
                    while ((match = re.exec(lineText)) !== null) {
                        var starStart = lineStart + match.index + match[1].length;
                        addRange(ranges, starStart, starStart + 1);
                        addRange(ranges, starStart + match[0].length - match[1].length - 1,
                                 starStart + match[0].length - match[1].length);
                    }

                    re = /(^|[^_])_([^_\n]+)_(?!_)/g;
                    while ((match = re.exec(lineText)) !== null) {
                        var underscoreStart = lineStart + match.index + match[1].length;
                        addRange(ranges, underscoreStart, underscoreStart + 1);
                        addRange(ranges,
                                 underscoreStart + match[0].length - match[1].length - 1,
                                 underscoreStart + match[0].length - match[1].length);
                    }

                    re = /\[([^\]]+)\]\(((?:\\.|[^)])+)\)/g;
                    while ((match = re.exec(lineText)) !== null) {
                        var start = lineStart + match.index;
                        var textStart = start + 1;
                        var textEnd = textStart + match[1].length;
                        var end = start + match[0].length;
                        addRange(ranges, start, textStart);
                        addRange(ranges, textEnd, end);
                    }

                    ranges.sort(function(a, b) { return a.start - b.start; });
                    return ranges;
                }

                function hiddenRangesAt(position) {
                    var bounds = lineBounds(position);
                    return hiddenRangesForLine(text.slice(bounds.start, bounds.end), bounds.start);
                }

                function skipHiddenForward(position) {
                    var pos = position;
                    var ranges = hiddenRangesAt(pos);
                    for (var i = 0; i < ranges.length; i++) {
                        if (pos >= ranges[i].start && pos < ranges[i].end) {
                            pos = ranges[i].end;
                            i = -1;
                        }
                    }
                    return pos;
                }

                function skipHiddenBackward(position) {
                    var pos = position;
                    var ranges = hiddenRangesAt(pos);
                    for (var i = ranges.length - 1; i >= 0; i--) {
                        if (pos > ranges[i].start && pos <= ranges[i].end) {
                            pos = ranges[i].start;
                            i = ranges.length;
                        }
                    }
                    return pos;
                }

                function moveCursorVisibly(direction) {
                    if (selectionStart !== selectionEnd) {
                        cursorPosition = direction > 0
                            ? Math.max(selectionStart, selectionEnd)
                            : Math.min(selectionStart, selectionEnd);
                        return;
                    }

                    var pos = Math.max(0, Math.min(text.length, cursorPosition + direction));
                    cursorPosition = direction > 0
                        ? skipHiddenForward(pos)
                        : skipHiddenBackward(pos);
                }

                function deleteParagraphBreakBehindCursor() {
                    if (selectionStart !== selectionEnd || cursorPosition < 2)
                        return false;

                    if (text.slice(cursorPosition - 2, cursorPosition) !== "\n\n")
                        return false;

                    var start = cursorPosition - 2;
                    remove(start, cursorPosition);
                    cursorPosition = start;
                    return true;
                }

                Keys.priority: Keys.BeforeItem
                Keys.onPressed: function(event) {
                    var pasteKey = (event.key === Qt.Key_V)
                        && (event.modifiers & Qt.ControlModifier)
                        && !(event.modifiers & (Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier));
                    var shiftInsert = (event.key === Qt.Key_Insert)
                        && (event.modifiers & Qt.ShiftModifier)
                        && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier));
                    if ((pasteKey || shiftInsert) && pasteClipboardUrlAsMarkdownLink()) {
                        event.accepted = true;
                        return;
                    }

                    var returnKey = event.key === Qt.Key_Return || event.key === Qt.Key_Enter;
                    var commandModifier = event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier);
                    if (returnKey && !commandModifier) {
                        replaceSelectionWith((event.modifiers & Qt.ShiftModifier) ? "\n" : "\n\n");
                        event.accepted = true;
                    } else if (!commandModifier && event.key === Qt.Key_Backspace
                               && deleteParagraphBreakBehindCursor()) {
                        event.accepted = true;
                    } else if (!commandModifier && !(event.modifiers & Qt.ShiftModifier)
                               && event.key === Qt.Key_Right) {
                        moveCursorVisibly(1);
                        event.accepted = true;
                    } else if (!commandModifier && !(event.modifiers & Qt.ShiftModifier)
                               && event.key === Qt.Key_Left) {
                        moveCursorVisibly(-1);
                        event.accepted = true;
                    }
                }

                onTextChanged: {
                    if (backend.documentText !== text)
                        backend.documentText = text;
                }

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: "# Start writing"
                    visible: editor.text.length === 0 && !editor.activeFocus
                    color: win.mutedColor
                    font.family: editor.font.family
                    font.pixelSize: editor.font.pixelSize
                    font.weight: editor.font.weight
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
                font.pixelSize: 18
                font.weight: Font.Normal
                renderType: TextEdit.NativeRendering
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
