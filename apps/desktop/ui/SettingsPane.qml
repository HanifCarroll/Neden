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
    readonly property AppSettings study: session.study.settings
    function saveSettings() {
        return study.applyDraft({
            endpoint: endpointField.text, model: modelField.text, dictionary: dictionaryField.text,
            automaticSubtitles: automaticField.checked, sourceLanguage: sourceLanguage.text,
            explanationLanguage: explanationLanguage.text, includeSecondary: includeSecondary.checked,
            noKeyRequired: noKeyRequired.checked, primaryProvider: primaryProvider.currentValue,
            fallbackEnabled: fallbackEnabled.checked, wordProvider: wordProvider.currentValue,
            wordRegion: wordRegion.text
        });
    }
    property string appearanceMessage: ""
    function restoreAppearanceControls() {
        primarySize.value = appearance.primarySize;
        secondarySize.value = appearance.secondarySize;
        bottomMargin.value = appearance.bottomMargin;
        outline.checked = appearance.outline;
        backgroundOption.checked = appearance.background;
        bold.checked = appearance.bold;
        textColor.text = appearance.textColor;
        secondaryColor.text = appearance.secondaryColor;
        trackGap.value = appearance.gap;
        reverseOrder.checked = appearance.reversed;
        selectable.checked = appearance.selectable;
        bottomPercent.value = appearance.bottomPercent;
    }
    function previewAppearance() {
        const values = pane.appearance.snapshot();
        values.primarySize = Math.round(primarySize.value);
        values.secondarySize = Math.round(secondarySize.value);
        values.bottomMargin = Math.round(bottomMargin.value);
        values.outline = outline.checked;
        values.background = backgroundOption.checked;
        values.bold = bold.checked;
        values.textColor = textColor.text;
        values.secondaryColor = secondaryColor.text;
        values.gap = Math.round(trackGap.value);
        values.reversed = reverseOrder.checked;
        values.selectable = selectable.checked;
        values.bottomPercent = bottomPercent.value;
        return pane.appearance.preview(values);
    }
    spacing: Theme.space4
    TabBar {
        id: categories
        onCurrentIndexChanged: {
            if (currentIndex !== 0) {
                pane.appearance.cancelPreview();
                pane.restoreAppearanceControls();
            }
        }
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
                onMoved: pane.previewAppearance()
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
                onMoved: pane.previewAppearance()
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
                onMoved: pane.previewAppearance()
                value: pane.appearance.bottomMargin
                Layout.fillWidth: true
                Accessible.name: "Subtitle bottom margin"
            }
            CheckBox {
                id: outline
                text: "Dark outline"
                onToggled: pane.previewAppearance()
                checked: pane.appearance.outline
            }
            CheckBox {
                id: bold
                text: "Semibold text"
                onToggled: pane.previewAppearance()
                checked: pane.appearance.bold
            }
            CheckBox {
                id: backgroundOption
                text: "Compact dark background"
                onToggled: pane.previewAppearance()
                checked: pane.appearance.background
            }
            Label {
                text: "Primary text color (hex)"
                color: Theme.ink
            }
            SearchField {
                id: textColor
                onTextEdited: pane.previewAppearance()
                text: pane.appearance.textColor
                Layout.fillWidth: true
                Accessible.name: "Subtitle text color"
                maximumLength: 7
            }
            Label {
                text: "Secondary text color (hex)"
                color: Theme.ink
            }
            SearchField {
                id: secondaryColor
                text: pane.appearance.secondaryColor
                onTextEdited: pane.previewAppearance()
                maximumLength: 7
                Layout.fillWidth: true
                Accessible.name: "Secondary subtitle color"
            }
            Label {
                text: "Space between tracks · " + Math.round(trackGap.value) + " px"
                color: Theme.ink
            }
            ControlSlider {
                id: trackGap
                from: 0
                to: 80
                stepSize: 1
                value: pane.appearance.gap
                onMoved: pane.previewAppearance()
                Layout.fillWidth: true
                Accessible.name: "Space between subtitle tracks"
            }
            Label {
                text: "Additional bottom offset · " + Math.round(bottomPercent.value) + "% of picture"
                color: Theme.ink
            }
            ControlSlider {
                id: bottomPercent
                from: 0
                to: 40
                stepSize: 1
                value: pane.appearance.bottomPercent
                onMoved: pane.previewAppearance()
                Layout.fillWidth: true
                Accessible.name: "Relative subtitle bottom offset"
            }
            CheckBox {
                id: reverseOrder
                text: "Display secondary subtitles above primary"
                checked: pane.appearance.reversed
                onToggled: pane.previewAppearance()
            }
            CheckBox {
                id: selectable
                text: "Selectable subtitles and explanation controls"
                checked: pane.appearance.selectable
                onToggled: pane.previewAppearance()
            }
            Label {
                text: "Preview changes in the video. Apply to keep them; closing Settings restores the saved appearance."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.space2
                ActionButton {
                    text: "Preview"
                    onClicked: pane.appearanceMessage = pane.previewAppearance() ? "Previewing. Apply to keep these changes." : "Check subtitle sizes and hex colors."
                }
                ActionButton {
                    text: "Apply appearance"
                    primary: true
                    onClicked: pane.appearanceMessage = pane.previewAppearance() && pane.appearance.applyPreview() ? "Appearance saved." : "Couldn’t save appearance."
                }
                ActionButton {
                    text: "Restore defaults"
                    onClicked: {
                        pane.appearance.reset();
                        pane.restoreAppearanceControls();
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
                objectName: "aiEndpointField"
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
                objectName: "aiModelField"
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
            Label {
                text: "Explanation languages"
                color: Theme.ink
                font.pixelSize: Theme.title
            }
            SearchField {
                id: sourceLanguage
                text: pane.study.sourceLanguage
                placeholderText: "Source language"
                Layout.fillWidth: true
                Accessible.name: "Source language"
            }
            SearchField {
                id: explanationLanguage
                text: pane.study.explanationLanguage
                placeholderText: "Explanation language"
                Layout.fillWidth: true
                Accessible.name: "Explanation language"
            }
            CheckBox {
                id: includeSecondary
                text: "Use overlapping secondary subtitles as context"
                checked: pane.study.includeSecondary
            }
            CheckBox {
                id: noKeyRequired
                text: "Configured endpoint does not require an API key"
                checked: pane.study.noKeyRequired
            }
            Label {
                text: "Primary explanation provider"
                color: Theme.muted
            }
            ChoiceBox {
                id: primaryProvider
                model: pane.study.primaryProviders
                textRole: "name"
                valueRole: "id"
                currentIndex: { for (let i=0; i<count; ++i) if (model[i].id === pane.study.primaryProvider) return i; return 0; }
                Layout.fillWidth: true
                Accessible.name: "Primary explanation provider"
            }
            CheckBox {
                id: fallbackEnabled
                text: "Use the other provider after 5 seconds without an answer"
                checked: pane.study.fallbackEnabled
            }
            Label {
                text: "Fallback is optional and may bill both attempts. It never switches after an answer starts. Each provider needs its own saved key."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: "Single-click word meanings"
                color: Theme.ink
                font.pixelSize: Theme.title
            }
            ChoiceBox {
                id: wordProvider
                objectName: "wordProviderChoice"
                model: pane.study.wordProviders
                textRole: "name"
                valueRole: "id"
                currentIndex: { for (let i=0; i<count; ++i) if (model[i].id === pane.study.wordProvider) return i; return 0; }
                Layout.fillWidth: true
                Accessible.name: "Word translation provider"
            }
            SearchField {
                id: wordRegion
                text: pane.study.wordRegion
                placeholderText: "Microsoft region (if required)"
                Layout.fillWidth: true
                Accessible.name: "Microsoft Translator region"
            }
            Label {
                text: "Contextual AI uses your configured AI model and saved key. A single click sends the word, its subtitle and nearby lines to show the meaning used in the scene, followed by other common meanings. Requests may incur charges. Double-click or drag a phrase for a full explanation."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ActionButton {
                text: "Save settings"
                objectName: "saveSettingsButton"
                onClicked: pane.saveSettings()
            }
            Label {
                text: "Secure service credentials"
                color: Theme.ink
                font.pixelSize: Theme.title
            }
            ChoiceBox {
                id: keyService
                model: pane.study.credentialServices
                textRole: "name"
                valueRole: "id"
                Layout.fillWidth: true
                Accessible.name: "Credential service"
            }
            SearchField {
                id: newKey
                text: ""
                echoMode: TextInput.Password
                maximumLength: 512
                placeholderText: "New API key"
                Layout.fillWidth: true
                Accessible.name: "New API key"
            }
            RowLayout {
                ActionButton {
                    text: "Save key in Keychain"
                    enabled: newKey.text.length > 0
                    onClicked: {
                        if (keyService.currentValue !== "ai" || pane.saveSettings()) {
                            pane.study.saveServiceKey(keyService.currentValue, newKey.text);
                            newKey.clear();
                        }
                    }
                }
                ActionButton {
                    text: "Connect saved key"
                    onClicked: {
                        if (keyService.currentValue !== "ai" || pane.saveSettings())
                            pane.study.connectServiceKey(keyService.currentValue);
                    }
                }
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
