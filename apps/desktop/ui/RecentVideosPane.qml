pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: pane
    required property RecentVideos recentVideos
    required property LearningSession session
    signal locateVideo(url oldUrl)
    signal openVideo
    signal gettingStarted

    RecentVideosFilter {
        id: historyView
        history: pane.recentVideos
        includeFinished: historyTabs.currentIndex === 1
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 640)
        height: Math.min(parent.height - 64, implicitHeight)
        spacing: Theme.space5
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space3
            Image {
                source: "qrc:/branding/neden.png"
                sourceSize: Qt.size(64, 64)
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
            }
            Label {
                text: "Neden"
                color: Theme.ink
                font.pixelSize: Theme.heading
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            ActionButton {
                objectName: "openVideoButton"
                text: "Open video…"
                onClicked: pane.openVideo()
            }
        }
        Label {
            visible: pane.recentVideos.error.length > 0
            text: pane.recentVideos.error
            textFormat: Text.PlainText
            color: Theme.error
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ActionButton {
            text: "Getting started"
            Accessible.description: "Learn how to open videos, choose subtitles, and connect AI"
            onClicked: pane.gettingStarted()
        }
        SegmentBar {
            id: historyTabs
            Layout.fillWidth: true
            SegmentButton {
                objectName: "continueWatchingTab"
                text: "Continue watching"
                width: historyTabs.availableWidth / 2
            }
            SegmentButton {
                objectName: "historyTab"
                text: "History"
                width: historyTabs.availableWidth / 2
            }
        }
        Label {
            visible: recentList.count === 0
            text: pane.recentVideos.count === 0 ? "Open a video, choose your subtitles, then click a word to explore its meaning. Playback works without an AI connection." : "You're all caught up. Finished videos are in History."
            color: Theme.muted
            font.pixelSize: Theme.body
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ListView {
            id: recentList
            objectName: "recentVideosList"
            visible: count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 360)
            Layout.minimumHeight: 48
            clip: true
            spacing: Theme.space1
            model: historyView
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: entry
                objectName: "resumeVideoButton"
                required property int index
                required property string mediaTitle
                required property url mediaUrl
                required property string filePath
                required property real resumePosition
                required property real durationSeconds
                required property bool fileAvailable
                required property bool finished
                width: recentList.width
                height: 48
                padding: Theme.space3
                hoverEnabled: true
                Accessible.name: (entry.fileAvailable ? (entry.finished ? "Play again: " : "Resume ") : "Unavailable: ") + entry.mediaTitle
                onClicked: {
                    if (entry.fileAvailable)
                        pane.session.openVideo(entry.mediaUrl);
                }
                ToolTip.visible: hovered && !removeButton.hovered && !locateButton.hovered && !finishButton.hovered
                ToolTip.delay: 800
                ToolTip.text: entry.fileAvailable ? entry.filePath : "Video unavailable. Reconnect its drive or remove it from this list."
                background: Rectangle {
                    radius: Theme.controlRadius
                    color: entry.hovered || entry.activeFocus ? Theme.hover : entry.index === 0 ? Theme.surface : "transparent"
                    border.color: entry.activeFocus ? Theme.accent : "transparent"
                }
                contentItem: RowLayout {
                    spacing: Theme.space3
                    Image {
                        source: "qrc:/icons/" + (entry.index === 0 ? "rotate-ccw" : "play") + ".svg"
                        sourceSize: Qt.size(18, 18)
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        opacity: entry.fileAvailable ? 0.65 : 0.3
                    }
                    Label {
                        text: (entry.finished ? "Play again · " : (entry.index === 0 && entry.resumePosition > 0 ? "Resume " : "")) + entry.mediaTitle
                        textFormat: Text.PlainText
                        color: entry.fileAvailable ? Theme.ink : Theme.muted
                        font.pixelSize: Theme.body
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: entry.fileAvailable ? (entry.finished ? "Finished" : Theme.clock(entry.resumePosition)) : "Unavailable"
                        color: Theme.muted
                        font.pixelSize: Theme.caption
                    }
                    ActionButton {
                        id: finishButton
                        objectName: "markFinishedButton"
                        text: entry.finished ? "Mark unfinished" : "Mark as finished"
                        flat: true
                        implicitHeight: 30
                        font.pixelSize: Theme.caption
                        onClicked: pane.recentVideos.setFinished(entry.mediaUrl, !entry.finished)
                    }
                    ActionButton {
                        id: locateButton
                        text: "Locate…"
                        visible: !entry.fileAvailable
                        onClicked: pane.locateVideo(entry.mediaUrl)
                    }
                    ActionButton {
                        id: removeButton
                        objectName: "removeRecentButton"
                        text: "Remove"
                        flat: true
                        implicitHeight: 30
                        font.pixelSize: Theme.caption
                        Accessible.name: "Remove " + entry.mediaTitle + " from recent videos"
                        onClicked: pane.recentVideos.remove(entry.mediaUrl)
                    }
                }
            }
        }
    }
}
