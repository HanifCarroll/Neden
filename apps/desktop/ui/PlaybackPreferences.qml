pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: pane
    required property LearningSession session
    implicitHeight: groups.implicitHeight
    ColumnLayout {
        id: groups
        width: parent.width
        spacing: Theme.space5
        SettingsGroup {
            title: "Playback history"
            Layout.fillWidth: true
            description: "Positions, subtitle tracks and picture adjustments are saved for local videos. Network URLs stay in the current queue."
            CheckBox {
                text: "Remember recently played videos"
                checked: pane.session.preferences.rememberHistory
                onToggled: pane.session.preferences.rememberHistory = checked
            }
            ActionButton {
                text: "Clear playback history…"
                onClicked: confirmation.open()
            }
        }
        SettingsGroup {
            title: "Playback speed"
            Layout.fillWidth: true
            CheckBox {
                text: "Keep speed when opening another video"
                checked: pane.session.preferences.rememberSpeed
                onToggled: pane.session.preferences.rememberSpeed = checked
            }
        }
    }
    Dialog {
        id: confirmation
        parent: Overlay.overlay
        title: "Clear playback history?"
        modal: true
        anchors.centerIn: parent
        width: Math.min(440, pane.width)
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label {
            text: "Saved positions, subtitle choices and per-video settings will be removed. Your files are kept."
            wrapMode: Text.WordWrap
            color: Theme.ink
        }
        onAccepted: pane.session.clearHistory()
    }
}
