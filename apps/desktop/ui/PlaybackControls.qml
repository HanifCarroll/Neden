import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: controls
    required property LearningSession session
    property bool mutedAudio: true
    signal fullscreen
    spacing: 0
    Slider {
        id: timeline
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        from: 0
        to: Math.max(1, controls.session.duration)
        value: controls.session.position
        onMoved: controls.session.seek(value)
        Accessible.name: "Playback position"
        background: Rectangle {
            x: timeline.leftPadding
            y: timeline.topPadding + timeline.availableHeight / 2 - height / 2
            width: timeline.availableWidth
            height: 3
            radius: 2
            color: Theme.border
            Rectangle {
                width: timeline.visualPosition * parent.width
                height: 3
                radius: 2
                color: Theme.accent
            }
        }
        handle: Rectangle {
            x: timeline.leftPadding + timeline.visualPosition * (timeline.availableWidth - width)
            y: timeline.topPadding + timeline.availableHeight / 2 - height / 2
            width: 10
            height: 10
            radius: 5
            color: Theme.accent
        }
    }
    RowLayout {
        Layout.preferredHeight: 48
        spacing: 8
        ActionButton {
            primary: true
            text: controls.session.paused ? "Play" : "Pause"
            glyph: controls.session.paused ? "play" : "pause"
            onClicked: controls.session.toggle()
        }
        ActionButton {
            flat: true
            text: controls.session.replaying ? "Stop replay" : "Replay sentence"
            onClicked: controls.session.replay()
        }
        Label {
            text: Theme.clock(controls.session.position) + " / " + Theme.clock(controls.session.duration)
            color: Theme.muted
            font.pixelSize: 12
        }
        Item {
            Layout.fillWidth: true
        }
        ComboBox {
            model: ["0.75×", "1.0×", "1.25×", "1.5×"]
            currentIndex: 1
            font.pixelSize: 12
            implicitWidth: 82
            background: Rectangle {
                color: "transparent"
            }
            onActivated: controls.session.rate([0.75, 1, 1.25, 1.5][currentIndex])
            Accessible.name: "Playback speed"
        }
        ActionButton {
            flat: true
            text: controls.mutedAudio ? "Unmute" : "Mute"
            onClicked: {
                controls.mutedAudio = !controls.mutedAudio;
                controls.session.volume(controls.mutedAudio ? 0 : 60);
            }
        }
        ActionButton {
            flat: true
            text: "Fullscreen"
            onClicked: controls.fullscreen()
        }
    }
}
