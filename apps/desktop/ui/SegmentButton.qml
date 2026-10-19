import QtQuick
import QtQuick.Controls

TabButton {
    id: control
    implicitHeight: Theme.controlHeight
    font.pixelSize: Theme.body
    contentItem: Text {
        text: control.text
        color: control.checked ? Theme.ink : Theme.muted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: Theme.controlRadius
        color: control.checked ? Theme.elevated : "transparent"
        border.color: control.activeFocus ? Theme.accent : "transparent"
    }
}
