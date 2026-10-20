pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: group
    property string title: ""
    property string description: ""
    default property alias settings: rows.data
    spacing: Theme.space3
    Label {
        visible: group.title.length > 0
        text: group.title
        font.pixelSize: Theme.body
        font.weight: Font.DemiBold
        color: Theme.ink
        Layout.fillWidth: true
    }
    Frame {
        Layout.fillWidth: true
        padding: Theme.space4
        background: Rectangle {
            color: Theme.surface
            radius: Theme.controlRadius
            border.color: Theme.border
        }
        contentItem: ColumnLayout {
            id: rows
            spacing: Theme.space4
        }
    }
    Label {
        visible: group.description.length > 0
        text: group.description
        color: Theme.muted
        font.pixelSize: Theme.caption
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
