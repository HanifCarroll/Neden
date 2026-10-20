import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Rectangle {
    id: controls
    property ThumbnailService thumbnails: null
    required property LearningSession session
    property bool sidebarOpen: false
    readonly property bool compact: width < 760
    readonly property bool hovered: controlsHover.hovered
    HoverHandler {
        id: controlsHover
        blocking: false
    }
    readonly property bool editing: timeline.activeFocus || speed.activeFocus || volume.activeFocus
    readonly property bool interacting: timeline.pressed || volume.pressed || speed.popup.visible
    signal fullscreen
    signal toggleSidebar
    implicitHeight: 72
    color: Theme.surface
    Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        spacing: Theme.space2
        IconButton {
            visible: !controls.compact
            objectName: "previousSubtitleButton"
            name: "skip-back"
            description: "Play previous subtitle · pause at end"
            enabled: controls.session.canPreviousSubtitle
            onClicked: controls.session.previousSubtitle()
        }
        IconButton {
            objectName: "playPauseButton"
            name: controls.session.paused ? "play" : "pause"
            description: controls.session.paused ? "Play" : "Pause"
            implicitWidth: 42
            onClicked: controls.session.toggle()
        }
        IconButton {
            visible: !controls.compact
            objectName: "nextSubtitleButton"
            name: "skip-forward"
            description: "Play next subtitle · pause at end"
            enabled: controls.session.canNextSubtitle
            onClicked: controls.session.nextSubtitle()
        }
        IconButton {
            visible: !controls.compact
            objectName: "replaySubtitleButton"
            name: "rotate-ccw"
            description: controls.session.replaying ? "Stop replay" : "Replay subtitle once · pause at end"
            enabled: controls.session.activeCue >= 0 || controls.session.replaying
            checked: controls.session.replaying
            onClicked: controls.session.replay()
        }
        Label {
            text: Theme.clock(controls.session.position)
            color: Theme.muted
            font.pixelSize: Theme.caption
            Layout.preferredWidth: implicitWidth + Theme.space2
            horizontalAlignment: Text.AlignHCenter
        }
        ControlSlider {
            id: timeline
            objectName: "timelineSlider"
            Layout.fillWidth: true
            Layout.minimumWidth: 80
            from: 0
            to: Math.max(1, controls.session.duration)
            value: controls.session.position
            enabled: controls.session.seekable
            onMoved: {
                if (pressed)
                    controls.session.previewSeek(value);
                else
                    controls.session.finishSeek(value);
            }
            onPressedChanged: {
                if (!pressed)
                    controls.session.finishSeek(value);
            }
            readonly property real hoverTime: Math.max(0, Math.min(to, from + timelineHover.point.position.x / width * (to - from)))
            HoverHandler {
                id: timelineHover
                onPointChanged: previewDelay.restart()
                onHoveredChanged: {
                    if (!hovered && controls.thumbnails)
                        controls.thumbnails.clear();
                }
            }
            Timer {
                id: previewDelay
                interval: 150
                onTriggered: {
                    if (timelineHover.hovered && controls.thumbnails)
                        controls.thumbnails.request(controls.session.mediaFile, timeline.hoverTime);
                }
            }
            ToolTip {
                objectName: "seekPreview"
                visible: timeline.hovered || timeline.pressed
                padding: Theme.space2
                background: Rectangle {
                    objectName: "seekPreviewBackground"
                    color: Theme.surface
                    border.color: Theme.border
                    radius: Theme.controlRadius
                }
                contentItem: Column {
                    spacing: 6
                    Image {
                        objectName: "seekPreviewImage"
                        source: controls.thumbnails ? controls.thumbnails.imageUrl : ""
                        visible: source.toString().length > 0
                        width: 240
                        height: visible ? 135 : 0
                        fillMode: Image.PreserveAspectFit
                        cache: false
                    }
                    Label {
                        objectName: "seekPreviewTime"
                        width: 240
                        text: Theme.clock(timeline.hoverTime)
                        color: Theme.ink
                        font.pixelSize: Theme.body
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
            Accessible.name: "Playback position"
        }
        Label {
            visible: !controls.compact
            text: "−" + Theme.clock(Math.max(0, controls.session.duration - controls.session.position))
            color: Theme.muted
            font.pixelSize: Theme.caption
            Layout.preferredWidth: implicitWidth + Theme.space2
            horizontalAlignment: Text.AlignHCenter
        }
        IconButton {
            name: controls.session.volumeLevel === 0 ? "volume-x" : "volume-2"
            description: controls.session.volumeLevel === 0 ? "Unmute" : "Mute"
            onClicked: controls.session.toggleMute()
        }
        ControlSlider {
            id: volume
            visible: !controls.compact
            objectName: "volumeSlider"
            Layout.preferredWidth: Math.min(140, controls.width * 0.1)
            from: 0
            to: 100
            stepSize: 1
            value: controls.session.volumeLevel
            onMoved: controls.session.volume(value)
            Accessible.name: "Volume"
            ToolTip.visible: hovered || pressed
            ToolTip.text: Math.round(value) + "%"
        }
        Rectangle {
            visible: !controls.compact
            implicitWidth: 1
            implicitHeight: 24
            color: Theme.border
            Layout.leftMargin: Theme.space2
        }
        ChoiceBox {
            id: speed
            objectName: "speedChoice"
            model: ["0.5×", "0.75×", "1.0×", "1.25×", "1.5×", "1.75×", "2.0×"]
            displayText: controls.session.playbackSpeed.toFixed(2).replace(/0$/, "") + "×"
            currentIndex: [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2].indexOf(controls.session.playbackSpeed)
            implicitWidth: 78
            background: Rectangle {
                color: speed.hovered ? Theme.hover : "transparent"
                radius: 6
                border.color: speed.activeFocus ? Theme.accent : "transparent"
            }
            onActivated: {
                controls.session.rate([0.5, 0.75, 1, 1.25, 1.5, 1.75, 2][currentIndex]);
                controls.forceActiveFocus();
            }
            Keys.onSpacePressed: event => {
                if (!popup.visible) {
                    controls.session.toggle();
                    event.accepted = true;
                } else {
                    event.accepted = false;
                }
            }
            Keys.onReleased: event => {
                if (event.key === Qt.Key_Space && !popup.visible)
                    event.accepted = true;
            }
            Accessible.name: "Playback speed"
        }
        IconButton {
            objectName: "toggleSidebarButton"
            name: "captions"
            description: controls.sidebarOpen ? "Hide sidebar" : "Show transcript"
            checked: controls.sidebarOpen
            onClicked: controls.toggleSidebar()
        }
        IconButton {
            name: "maximize"
            description: "Fullscreen"
            onClicked: controls.fullscreen()
        }
    }
}
