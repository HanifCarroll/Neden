import QtQuick
import QtQuick.Controls
import Neden

Slider {
    id: control
    implicitHeight: Theme.controlHeight
    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 3
        radius: 2
        color: Theme.border
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: control.enabled ? Theme.accent : Theme.muted
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 10
        height: 10
        radius: 5
        color: control.activeFocus ? Theme.ink : Theme.accent
    }
}
