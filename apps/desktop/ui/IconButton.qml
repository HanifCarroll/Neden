import QtQuick
import QtQuick.Controls

ToolButton {
    id: control
    required property string name
    required property string description
    implicitWidth: 36
    implicitHeight: 36
    hoverEnabled: true
    HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
    opacity: enabled ? 1 : 0.3
    Accessible.name: description
    ToolTip.visible: hovered
    ToolTip.delay: 600
    ToolTip.text: description
    contentItem: Image {
        source: "qrc:/icons/" + control.name + ".svg"
        sourceSize: Qt.size(24, 24)
        fillMode: Image.PreserveAspectFit
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
    }
    padding: 8
    background: Rectangle {
        radius: 6
        color: control.down || control.checked ? Theme.selected : control.hovered ? Theme.hover : "transparent"
        border.color: control.activeFocus ? Theme.accent : "transparent"
    }
}
