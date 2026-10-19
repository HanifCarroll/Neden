pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: pane
    required property RecentVideos recentVideos
    required property LearningSession session
    signal openVideo
    spacing: 20

    Label {
        text: "Pick up where you left off"
        color: Theme.ink
        font.pixelSize: 30
        font.weight: Font.DemiBold
        Layout.topMargin: 24
    }
    Label {
        text: "Open a video and add up to two subtitle tracks to watch and learn."
        color: Theme.muted
        font.pixelSize: Theme.body
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    RowLayout {
        Label {
            text: "Recently played"
            color: Theme.ink
            font.pixelSize: Theme.title
            font.weight: Font.Medium
            Layout.fillWidth: true
        }
        Label {
            text: "Your place is saved locally"
            color: Theme.muted
            font.pixelSize: Theme.caption
        }
    }
    Label {
        visible: pane.recentVideos.error.length > 0
        text: pane.recentVideos.error
        textFormat: Text.PlainText
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Rectangle {
        visible: pane.recentVideos.count === 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: Theme.panelRadius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 18
            Label {
                text: "Your next listening session starts here"
                color: Theme.ink
                font.pixelSize: 20
                Layout.alignment: Qt.AlignHCenter
            }
            Label {
                text: "Videos you open will appear here, ready to resume."
                color: Theme.muted
                Layout.alignment: Qt.AlignHCenter
            }
            ActionButton {
                text: "Open video"
                primary: true
                Layout.alignment: Qt.AlignHCenter
                onClicked: pane.openVideo()
            }
        }
    }
    ListView {
        id: recentList
        objectName: "recentVideosList"
        visible: pane.recentVideos.count > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: Theme.space3
        model: pane.recentVideos
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            id: entry
            required property string mediaTitle
            required property url mediaUrl
            required property string filePath
            required property real resumePosition
            required property real durationSeconds
            required property bool fileAvailable
            width: recentList.width
            height: entryContents.implicitHeight + 36
            radius: Theme.panelRadius
            color: Theme.surface
            border.color: Theme.border
            RowLayout {
                id: entryContents
                anchors.fill: parent
                anchors.margins: 18
                spacing: 24
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Label {
                        text: entry.mediaTitle
                        textFormat: Text.PlainText
                        color: Theme.ink
                        font.pixelSize: Theme.title
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: entry.filePath
                        textFormat: Text.PlainText
                        color: Theme.muted
                        font.pixelSize: Theme.caption
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                    Label {
                        text: !entry.fileAvailable ? "Video unavailable — reconnect its drive or open it again" : Theme.clock(entry.resumePosition) + " / " + Theme.clock(entry.durationSeconds)
                        color: Theme.muted
                        font.pixelSize: Theme.caption
                        textFormat: Text.PlainText
                    }
                }
                ActionButton {
                    objectName: "resumeVideoButton"
                    text: entry.resumePosition > 0 ? "Resume" : "Play"
                    primary: true
                    enabled: entry.fileAvailable
                    Accessible.name: "Resume " + entry.mediaTitle
                    onClicked: pane.session.openVideo(entry.mediaUrl)
                }
                ActionButton {
                    objectName: "removeRecentButton"
                    text: "Remove"
                    flat: true
                    Accessible.name: "Remove " + entry.mediaTitle + " from recent videos"
                    onClicked: pane.recentVideos.remove(entry.mediaUrl)
                }
            }
        }
    }
}
