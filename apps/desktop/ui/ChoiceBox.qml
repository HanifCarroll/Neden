pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Neden

ComboBox {
    id: control
    implicitHeight: Theme.controlHeight
    implicitWidth: 180
    leftPadding: Theme.space3
    rightPadding: Theme.space6
    font.pixelSize: Theme.body
    hoverEnabled: true
    property int textElide: Text.ElideRight
    contentItem: Text {
        text: control.displayText
        textFormat: Text.PlainText
        color: control.enabled ? Theme.ink : Theme.muted
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: control.textElide
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
        width: control.popup.availableWidth
        height: Math.max(Theme.controlHeight, contentItem.implicitHeight + topPadding + bottomPadding)
        leftPadding: Theme.space3
        rightPadding: Theme.space3
        topPadding: Theme.space2
        bottomPadding: Theme.space2
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textAt(row.index)
            textFormat: Text.PlainText
            font: control.font
            color: row.highlighted ? Theme.background : Theme.ink
            verticalAlignment: Text.AlignVCenter
            wrapMode: control.textRole.length ? Text.Wrap : Text.NoWrap
            elide: control.textRole.length ? Text.ElideNone : Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.controlRadius
            color: row.highlighted ? Theme.accent : Theme.surface
        }
    }
    popup: Popup {
        objectName: control.objectName + "Popup"
        y: control.height + Theme.space1
        width: control.textRole.length ? Math.max(control.width, Math.min(360, control.ApplicationWindow.window ? control.ApplicationWindow.window.width - Theme.space6 : 360)) : control.width
        margins: Theme.space2
        padding: Theme.space1
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        height: Math.min(implicitHeight, control.ApplicationWindow.window ? control.ApplicationWindow.window.height - Theme.space6 : implicitHeight)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
            radius: Theme.controlRadius
        }
    }
}
