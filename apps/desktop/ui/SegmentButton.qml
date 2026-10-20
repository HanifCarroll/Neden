import QtQuick
import QtQuick.Controls.Basic as Basic
import Neden

Basic.TabButton {
    id: control
    implicitHeight: Theme.controlHeight
    font.pixelSize: Theme.body
    contentItem: Text {
        text: control.text
        color: control.checked ? Theme.accent : Theme.muted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.hovered ? Theme.hover : "transparent"
        border.color: control.activeFocus ? Theme.accent : "transparent"
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            color: control.checked ? Theme.accent : "transparent"
        }
    }
}
