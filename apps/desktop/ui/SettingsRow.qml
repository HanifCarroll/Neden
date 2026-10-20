pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: row
    required property string title
    property string description: ""
    default property alias controls: controlRow.data
    readonly property bool stacked: width < 430
    implicitHeight: stacked ? labels.implicitHeight + Theme.space2 + controlRow.implicitHeight : Math.max(labels.implicitHeight, controlRow.implicitHeight)
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    ColumnLayout {
        id: labels
        width: row.stacked ? row.width : 150
        y: row.stacked ? 0 : (row.height - implicitHeight) / 2
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
        x: row.stacked ? 0 : labels.width + Theme.space5
        y: row.stacked ? labels.implicitHeight + Theme.space2 : (row.height - implicitHeight) / 2
        width: row.width - x
        spacing: Theme.space2
    }
}
