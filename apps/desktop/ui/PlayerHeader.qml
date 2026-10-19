import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: header
    property bool hasMedia: false
    property bool showingRecent: true
    signal openRecent
    signal backToVideo
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
            text: "Neden"
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
        objectName: "recentVideosButton"
        text: header.showingRecent ? "Back to video" : "Recent videos"
        visible: header.hasMedia
        flat: true
        onClicked: {
            if (header.showingRecent)
                header.backToVideo();
            else
                header.openRecent();
        }
    }
    ActionButton {
        text: "Load subtitles"
        enabled: header.hasMedia
        flat: true
        onClicked: subtitleMenu.open()
        Menu {
            id: subtitleMenu
            MenuItem {
                text: "Primary subtitles…"
                onTriggered: header.openSubtitles()
            }
            MenuItem {
                text: "Secondary subtitles…"
                onTriggered: header.openTranslation()
            }
        }
    }
    ActionButton {
        text: "Study"
        flat: true
        onClicked: header.openStudy()
    }
    ActionButton {
        text: "Dictionary"
        flat: true
        onClicked: header.openDictionary()
    }
    ActionButton {
        objectName: "settingsButton"
        text: "Settings"
        flat: true
        onClicked: header.openSettings()
    }
    ActionButton {
        objectName: "openVideoButton"
        text: "Open video"
        primary: true
        onClicked: header.openVideo()
    }
}
