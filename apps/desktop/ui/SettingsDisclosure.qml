pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Neden

Item {
    id: section
    required property string title
    property bool expanded: false
    default property alias settings: rows.data
    implicitHeight: body.implicitHeight
    ColumnLayout {
        id: body
        width: section.width
        spacing: Theme.space3
        ActionButton {
            objectName: section.objectName + "Toggle"
            text: (section.expanded ? "⌄  " : "›  ") + section.title
            checkable: true
            checked: section.expanded
            Accessible.name: section.title
            onClicked: section.expanded = !section.expanded
        }
        ColumnLayout {
            id: rows
            visible: section.expanded
            Layout.fillWidth: true
            spacing: Theme.space3
        }
    }
}
