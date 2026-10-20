pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

GridLayout {
    id: row
    required property string title
    property string description: ""
    default property alias controls: controlRow.data
    columns: width < 430 ? 1 : 2
    columnSpacing: Theme.space5
    rowSpacing: Theme.space2
    Layout.fillWidth: true
    ColumnLayout {
        Layout.preferredWidth: row.columns === 2 ? 150 : -1
        Layout.fillWidth: row.columns === 1
        spacing: Theme.space1
        Label {
            text: row.title
            color: Theme.ink
            font.pixelSize: Theme.body
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            visible: row.description.length > 0
            text: row.description
            color: Theme.muted
            font.pixelSize: Theme.caption
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
    RowLayout {
        id: controlRow
        Layout.fillWidth: true
        spacing: Theme.space2
    }
}
