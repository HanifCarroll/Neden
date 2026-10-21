pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: section
    required property string title
    required property var entries
    visible: entries.length > 0
    spacing: Theme.space2
    Label {
        text: section.title
        textFormat: Text.PlainText
        color: Theme.ink
        font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Repeater {
        model: section.entries
        delegate: ColumnLayout {
            id: usageEntry
            required property var modelData
            Layout.fillWidth: true
            spacing: Theme.space1
            Label {
                text: usageEntry.modelData.meaning
                textFormat: Text.PlainText
                color: Theme.ink
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: usageEntry.modelData.example + " — " + usageEntry.modelData.translation
                textFormat: Text.PlainText
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: usageEntry.modelData.formNote
                visible: text.length > 0
                textFormat: Text.PlainText
                color: Theme.muted
                font.pixelSize: Theme.caption
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
