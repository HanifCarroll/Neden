pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: group
    property string title: ""
    property string description: ""
    default property alias settings: rows.data
    implicitHeight: body.implicitHeight
    ColumnLayout {
        id: body
        width: group.width
        spacing: Theme.space3
        Label {
            visible: group.title.length > 0
            text: group.title
            font.pixelSize: Theme.body
            font.weight: Font.DemiBold
            color: Theme.ink
            Layout.fillWidth: true
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: rows.implicitHeight
            ColumnLayout {
                id: rows
                width: parent.width
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
}
