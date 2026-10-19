pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: pane
    required property SubtitleAppearance appearance
    required property LearningSession session
    required property FullscreenController fullscreenController
    readonly property StudyTools study: session.study
    property string appearanceMessage: ""
    spacing: Theme.space4
    TabBar {
        id: categories
        Layout.fillWidth: true
        SegmentButton {
            text: "Subtitles"
        }
        SegmentButton {
            text: "Playback & services"
        }
    }
    ScrollView {
        visible: categories.currentIndex === 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: Theme.space4
            Label {
                text: "Subtitle appearance"
                color: Theme.ink
                font.pixelSize: Theme.title
            }
            Label {
                text: "Both tracks use centered text with a dark outline. File formatting tags are removed so your style stays consistent."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: "Primary text · " + Math.round(primarySize.value) + " px"
                color: Theme.ink
            }
            ControlSlider {
                id: primarySize
                objectName: "primarySizeSlider"
                from: 16
                to: 64
                stepSize: 1
                value: pane.appearance.primarySize
                Layout.fillWidth: true
                Accessible.name: "Primary subtitle size"
            }
            Label {
                text: "Secondary text · " + Math.round(secondarySize.value) + " px"
                color: Theme.ink
            }
            ControlSlider {
                id: secondarySize
                from: 12
                to: 56
                stepSize: 1
                value: pane.appearance.secondarySize
                Layout.fillWidth: true
                Accessible.name: "Secondary subtitle size"
            }
            Label {
                text: "Bottom margin · " + Math.round(bottomMargin.value) + " px"
                color: Theme.ink
            }
            ControlSlider {
                id: bottomMargin
                from: 8
                to: 160
                stepSize: 1
                value: pane.appearance.bottomMargin
                Layout.fillWidth: true
                Accessible.name: "Subtitle bottom margin"
            }
            CheckBox {
                id: outline
                text: "Dark outline"
                checked: pane.appearance.outline
            }
            CheckBox {
                id: bold
                text: "Semibold text"
                checked: pane.appearance.bold
            }
            CheckBox {
                id: backgroundOption
                text: "Compact dark background"
                checked: pane.appearance.background
            }
            Label {
                text: "Text color (hex)"
                color: Theme.ink
            }
            SearchField {
                id: textColor
                text: pane.appearance.textColor
                Layout.fillWidth: true
                Accessible.name: "Subtitle text color"
                maximumLength: 7
            }
            RowLayout {
                ActionButton {
                    text: "Apply appearance"
                    primary: true
                    onClicked: pane.appearanceMessage = pane.appearance.configure(Math.round(primarySize.value), Math.round(secondarySize.value), Math.round(bottomMargin.value), outline.checked, backgroundOption.checked, bold.checked, textColor.text) ? "Appearance saved." : "Couldn’t save. Use a color such as #ffffff and check that your settings folder is writable."
                }
                ActionButton {
                    text: "Restore defaults"
                    onClicked: {
                        pane.appearance.reset();
                        primarySize.value = pane.appearance.primarySize;
                        secondarySize.value = pane.appearance.secondarySize;
                        bottomMargin.value = pane.appearance.bottomMargin;
                        outline.checked = pane.appearance.outline;
                        backgroundOption.checked = pane.appearance.background;
                        bold.checked = pane.appearance.bold;
                        textColor.text = pane.appearance.textColor;
                        pane.appearanceMessage = "Default appearance restored.";
                    }
                }
            }
            Label {
                text: pane.appearanceMessage
                wrapMode: Text.WordWrap
                color: Theme.muted
                Layout.fillWidth: true
            }
        }
    }
    ScrollView {
        visible: categories.currentIndex === 1
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: Theme.space4
            Label {
                visible: pane.fullscreenController.supportsNativeMode
                text: "Fullscreen"
                color: Theme.muted
            }
            ChoiceBox {
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
        visible: categories.currentIndex === 1
        text: pane.study.message
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        color: Theme.muted
    }
}
