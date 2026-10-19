import QtQuick
import QtQuick.Controls

TextField {
    id: control
    implicitHeight: 42
    leftPadding: 12
    rightPadding: 12
    color: Theme.ink
    font.pixelSize: 13
    placeholderTextColor: Theme.muted
    selectionColor: "#467b68"
    background: Rectangle {
        radius: 9
        color: Theme.background
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
