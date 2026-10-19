import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool primary: false
    property string glyph: ""
    implicitHeight: 38
    implicitWidth: label.implicitWidth + 28 + (glyph.length ? 24 : 0)
    hoverEnabled: true
    font.pixelSize: 13
    background: Rectangle {
        radius: 9
        color: control.primary ? Theme.accent : (control.hovered ? "#2b3739" : (control.flat ? "transparent" : Theme.elevated))
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
