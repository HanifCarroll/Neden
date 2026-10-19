pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: pane
    required property RecentVideos recentVideos
    required property LearningSession session
    signal openVideo

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
        Label {
            visible: pane.recentVideos.count === 0
            text: "Open a video to get started. Your recent videos will appear here."
            color: Theme.muted
            font.pixelSize: Theme.body
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ListView {
            id: recentList
            objectName: "recentVideosList"
            visible: pane.recentVideos.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 360)
            Layout.minimumHeight: 48
            clip: true
            spacing: Theme.space1
            model: pane.recentVideos
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
                width: recentList.width
                height: 48
                padding: Theme.space3
                hoverEnabled: true
                Accessible.name: (entry.fileAvailable ? "Resume " : "Unavailable: ") + entry.mediaTitle
                onClicked: {
                    if (entry.fileAvailable)
                        pane.session.openVideo(entry.mediaUrl);
                }
                ToolTip.visible: hovered
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
                        text: (entry.index === 0 && entry.resumePosition > 0 ? "Resume " : "") + entry.mediaTitle
                        textFormat: Text.PlainText
                        color: entry.fileAvailable ? Theme.ink : Theme.muted
                        font.pixelSize: Theme.body
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: entry.fileAvailable ? Theme.clock(entry.resumePosition) : "Unavailable"
                        color: Theme.muted
                        font.pixelSize: Theme.caption
                    }
                    ActionButton {
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
