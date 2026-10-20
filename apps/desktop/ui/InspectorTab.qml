import QtQuick
import QtQuick.Controls
import Neden

TabButton {
    id: control
    implicitHeight: 44
    contentItem: Text {
        text: control.text
        color: control.checked ? Theme.accent : Theme.muted
        font.pixelSize: Theme.body
        font.weight: control.checked ? Font.Medium : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: "transparent"
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 32
            height: 2
            color: Theme.accent
            visible: control.checked
        }
        border.color: control.activeFocus ? Theme.accent : "transparent"
    }
}
