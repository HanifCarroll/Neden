import QtQuick
import QtQuick.Controls
import Neden

Button {
    id: control
    property bool primary: false
    implicitHeight: Theme.controlHeight
    implicitWidth: label.implicitWidth + 28
    hoverEnabled: true
    HoverHandler {
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
    opacity: enabled ? 1 : 0.45
    font.pixelSize: Theme.body
    background: Rectangle {
        radius: Theme.controlRadius
        color: control.primary ? Theme.accent : (control.hovered ? Theme.hover : (control.flat ? "transparent" : Theme.elevated))
        opacity: control.down ? 0.75 : 1
        border.color: control.activeFocus ? Theme.accent : "transparent"
        border.width: 1
        Behavior on color {
            ColorAnimation {
                duration: 100
            }
        }
    }
    contentItem: Item {
        Row {
            anchors.centerIn: parent
            spacing: 8
            Text {
                id: label
                text: control.text
                font: control.font
                color: control.primary ? Theme.background : (control.flat ? Theme.muted : Theme.ink)
                textFormat: Text.PlainText
            }
        }
    }
}
