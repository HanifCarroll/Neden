pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    implicitHeight: Theme.controlHeight
    implicitWidth: 180
    leftPadding: Theme.space3
    rightPadding: Theme.space6
    font.pixelSize: Theme.body
    hoverEnabled: true
    contentItem: Text {
        text: control.displayText
        color: control.enabled ? Theme.ink : Theme.muted
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - width - Theme.space3
        anchors.verticalCenter: parent.verticalCenter
        text: "⌄"
        color: Theme.muted
        font.pixelSize: Theme.body
    }
    background: Rectangle {
        color: control.hovered ? Theme.hover : Theme.elevated
        radius: Theme.controlRadius
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
    delegate: ItemDelegate {
        id: row
        objectName: control.objectName + "Option" + index
        required property int index
        required property var modelData
        width: control.width
        height: Theme.controlHeight
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: row.modelData
            font: control.font
            color: row.highlighted ? Theme.background : Theme.ink
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.controlRadius
            color: row.highlighted ? Theme.accent : Theme.surface
        }
    }
    popup: Popup {
        objectName: control.objectName + "Popup"
        y: control.height + Theme.space1
        width: control.width
        padding: Theme.space1
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }
        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
            radius: Theme.controlRadius
        }
    }
}
