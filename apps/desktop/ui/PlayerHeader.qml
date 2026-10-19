import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: header
    signal openSettings
    signal openStudy
    signal openTranslation
    signal openVideo
    signal openSubtitles
    signal openDictionary
    implicitHeight: 64
    spacing: 12
    Rectangle {
        implicitWidth: 38
        implicitHeight: 38
        radius: 12
        color: Theme.accent
        Text {
            anchors.centerIn: parent
            text: "n"
            color: Theme.background
            font.pixelSize: 28
            font.bold: true
        }
    }
    ColumnLayout {
        spacing: 3
        Label {
            text: "neden"
            color: Theme.ink
            font.pixelSize: 23
            font.weight: Font.DemiBold
            font.letterSpacing: -0.6
        }
        Label {
            text: "Language learning player"
            color: Theme.muted
            font.pixelSize: 11
        }
    }
    Item {
        Layout.fillWidth: true
    }
    ActionButton {
        text: "Load subtitles"
        flat: true
        onClicked: subtitleMenu.open()
        Menu {
            id: subtitleMenu
            MenuItem { text: "Source subtitles"; onTriggered: header.openSubtitles() }
            MenuItem { text: "Translation subtitles"; onTriggered: header.openTranslation() }
        }
    }
    ActionButton { text: "Study"; flat: true; onClicked: header.openStudy() }
    ActionButton {
        text: "Dictionary"
        flat: true
        onClicked: header.openDictionary()
    }
    ActionButton { text: "Settings"; flat: true; onClicked: header.openSettings() }
    ActionButton {
        text: "Open video"
        primary: true
        onClicked: header.openVideo()
    }
}
