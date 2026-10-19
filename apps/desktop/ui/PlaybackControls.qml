import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: controls
    required property LearningSession session
    readonly property bool editing: timeline.activeFocus || speed.activeFocus || volume.activeFocus
    signal fullscreen
    spacing: 0
    ControlSlider {
        id: timeline
        Layout.fillWidth: true
        from: 0
        to: Math.max(1, controls.session.duration)
        value: controls.session.position
        onMoved: controls.session.seek(value)
        Accessible.name: "Playback position"
    }
    RowLayout {
        spacing: Theme.space2
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
            font.pixelSize: Theme.caption
        }
        Item {
            Layout.fillWidth: true
        }
        ChoiceBox {
            id: speed
            objectName: "speedChoice"
            model: ["0.75×", "1.0×", "1.25×", "1.5×"]
            currentIndex: 1
            implicitWidth: 96
            onActivated: controls.session.rate([0.75, 1, 1.25, 1.5][currentIndex])
            Accessible.name: "Playback speed"
        }
        ActionButton {
            flat: true
            text: controls.session.volumeLevel === 0 ? "Unmute" : "Mute"
            onClicked: controls.session.toggleMute()
        }
        ControlSlider {
            id: volume
            objectName: "volumeSlider"
            Layout.preferredWidth: 100
            from: 0
            to: 100
            stepSize: 1
            value: controls.session.volumeLevel
            onMoved: controls.session.volume(value)
            Accessible.name: "Volume"
            ToolTip.visible: hovered || pressed
            ToolTip.text: Math.round(value) + "%"
        }
        ActionButton {
            flat: true
            text: "Fullscreen"
            onClicked: controls.fullscreen()
        }
    }
}
