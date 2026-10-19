import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool primary: false
    property string glyph: ""
    implicitHeight: Theme.controlHeight
    implicitWidth: label.implicitWidth + 28 + (glyph.length ? 24 : 0)
    hoverEnabled: true
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
            Glyph {
                visible: control.glyph.length > 0
                name: control.glyph
                anchors.verticalCenter: parent.verticalCenter
                strokeColor: label.color
            }
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
