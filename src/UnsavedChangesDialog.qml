import QtQuick
import QtQuick.Controls

Dialog {
    id: root

    property string fileName: "Untitled.md"
    property bool darkMode: true
    property color textColor: darkMode ? "#d0d0d0" : "#42464c"
    property color strongTextColor: darkMode ? "#eeeeee" : "#222324"
    property color activeButtonColor: "#428bca"
    property int containerWidth: 420
    property int containerHeight: 320

    signal saveRequested()
    signal discardRequested()

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.NoAutoClose
    width: Math.min(420, containerWidth - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.round((containerHeight - height) / 2)
    padding: 20

    background: Rectangle {
        color: root.darkMode ? "#1a1a1a" : "#ffffff"
        border.color: root.darkMode ? "#343434" : "#d8d8d8"
        radius: 0
    }

    contentItem: Column {
        spacing: 12

        Label {
            text: "Unsaved changes"
            color: root.strongTextColor
            font.family: "iA Writer Mono S"
            font.pixelSize: 16
            font.bold: true
        }

        Label {
            width: parent.width
            text: "Save changes to " + root.fileName + " before closing?"
            color: root.textColor
            wrapMode: Text.Wrap
            font.family: "iA Writer Mono S"
            font.pixelSize: 13
        }
    }

    footer: Item {
        implicitHeight: dialogButtons.implicitHeight + 20

        Row {
            id: dialogButtons
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            SquareDialogButton {
                id: cancelButton
                text: "Cancel"
                darkMode: root.darkMode
                labelColor: root.textColor
                focus: true
                activeFocusOnTab: true
                onClicked: root.close()
                KeyNavigation.tab: discardButton
                KeyNavigation.right: discardButton
                KeyNavigation.left: saveButton
                KeyNavigation.backtab: saveButton
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                        root.close();
                        event.accepted = true;
                    }
                }
            }

            SquareDialogButton {
                id: discardButton
                text: "Discard"
                darkMode: root.darkMode
                labelColor: root.textColor
                activeFocusOnTab: true
                onClicked: {
                    root.close();
                    root.discardRequested();
                }
                KeyNavigation.tab: saveButton
                KeyNavigation.right: saveButton
                KeyNavigation.left: cancelButton
                KeyNavigation.backtab: cancelButton
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                        root.close();
                        root.discardRequested();
                        event.accepted = true;
                    }
                }
            }

            SquareDialogButton {
                id: saveButton
                text: "Save"
                primary: true
                darkMode: root.darkMode
                activeColor: root.activeButtonColor
                activeFocusOnTab: true
                onClicked: {
                    root.close();
                    root.saveRequested();
                }
                KeyNavigation.tab: cancelButton
                KeyNavigation.right: cancelButton
                KeyNavigation.left: discardButton
                KeyNavigation.backtab: discardButton
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
                        root.close();
                        root.saveRequested();
                        event.accepted = true;
                    }
                }
            }
        }
    }

    // Set initial focus to the save button (default action) when dialog opens
    onOpened: {
        saveButton.forceActiveFocus();
    }

    // Capture ESC key to close dialog
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.close();
            event.accepted = true;
        }
    }
}
