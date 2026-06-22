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
    closePolicy: Popup.NoAutoClose
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
                text: "Cancel"
                darkMode: root.darkMode
                labelColor: root.textColor
                onClicked: root.close()
            }

            SquareDialogButton {
                text: "Discard"
                darkMode: root.darkMode
                labelColor: root.textColor
                onClicked: {
                    root.close();
                    root.discardRequested();
                }
            }

            SquareDialogButton {
                text: "Save"
                primary: true
                darkMode: root.darkMode
                activeColor: root.activeButtonColor
                onClicked: {
                    root.close();
                    root.saveRequested();
                }
            }
        }
    }
}
