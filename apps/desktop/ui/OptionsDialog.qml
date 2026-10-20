pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Neden

Dialog {
    id: dialog
    modal: true
    padding: Theme.space5
    background: Rectangle {
        color: Theme.background
        border.color: Theme.border
        radius: Theme.panelRadius
    }
    header: Item {
        implicitHeight: 72
        Label {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space5
            anchors.verticalCenter: parent.verticalCenter
            text: dialog.title
            color: Theme.ink
            font.pixelSize: Theme.title
            font.weight: Font.DemiBold
        }
        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            name: "x"
            description: "Close " + dialog.title
            onClicked: dialog.close()
        }
    }
    footer: Item {
        implicitHeight: Theme.controlHeight + Theme.space5 * 2
        Rectangle {
            width: parent.width
            height: 1
            color: Theme.border
        }
        ActionButton {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space5
            anchors.verticalCenter: parent.verticalCenter
            text: "Done"
            onClicked: dialog.close()
        }
    }
}
