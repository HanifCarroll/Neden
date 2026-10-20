import QtQuick
import QtQuick.Controls
import Neden

TextField {
    id: control
    implicitHeight: Theme.controlHeight
    leftPadding: 12
    rightPadding: 12
    color: Theme.ink
    font.pixelSize: Theme.body
    placeholderTextColor: Theme.muted
    selectionColor: Theme.selection
    selectedTextColor: Theme.ink
    background: Rectangle {
        radius: Theme.controlRadius
        color: Theme.background
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
