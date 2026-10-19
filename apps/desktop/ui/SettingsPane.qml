pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: pane
    required property LearningSession session
    required property FullscreenController fullscreenController
    readonly property StudyTools study: session.study
    spacing: 16
    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 14
            Label {
                visible: pane.fullscreenController.supportsNativeMode
                text: "Fullscreen"
                color: Theme.muted
            }
            ComboBox {
                visible: pane.fullscreenController.supportsNativeMode
                Layout.fillWidth: true
                model: ["Instant · current desktop", "Native · separate Space"]
                currentIndex: pane.fullscreenController.nativeMode ? 1 : 0
                Accessible.name: "Fullscreen mode"
                onActivated: pane.fullscreenController.nativeMode = currentIndex === 1
            }
            Label {
                visible: pane.fullscreenController.supportsNativeMode
                text: "Saved automatically. Applies the next time you enter fullscreen."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: "AI API base URL"
                color: Theme.muted
            }
            SearchField {
                id: endpointField
                text: pane.study.endpoint
                Layout.fillWidth: true
                Accessible.name: "AI API base URL"
            }
            Label {
                text: "Exact model ID"
                color: Theme.muted
            }
            SearchField {
                id: modelField
                text: pane.study.model
                Layout.fillWidth: true
                Accessible.name: "AI model ID"
            }
            Label {
                text: "Dictionary URL · %s is the encoded selection"
                color: Theme.muted
            }
            SearchField {
                id: dictionaryField
                objectName: "dictionaryTemplate"
                text: pane.study.dictionaryTemplate
                Layout.fillWidth: true
                Accessible.name: "Dictionary URL template"
            }
            CheckBox {
                id: automaticField
                checked: pane.study.automaticSubtitles
                text: "Find and download Turkish hash matches on open"
            }
            Label {
                text: "Automatic lookup sends a video fingerprint to OpenSubtitles and uses its download allowance. It starts only when you open another video. Your original media and subtitle files are not overwritten."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ActionButton {
                text: "Save settings"
                onClicked: pane.study.configure(endpointField.text, modelField.text, dictionaryField.text, automaticField.checked)
            }
            ActionButton {
                text: "Connect AI Keychain"
                onClicked: pane.study.connectCredential(false)
            }
            ActionButton {
                text: "Connect OpenSubtitles Keychain"
                onClicked: pane.study.connectCredential(true)
            }
            Label {
                text: "Connect lets macOS ask for access to the saved Neden key. Approve in the system dialog. Keys are kept private for this app session; automatic lookups never prompt."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
    Label {
        text: pane.study.message
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        color: Theme.muted
    }
}
