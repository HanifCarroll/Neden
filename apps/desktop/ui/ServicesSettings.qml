pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: pane
    required property AppSettings settings
    required property HostedAccount account
    function providerIndex(providerOptions, providerId) {
        for (let optionIndex = 0; optionIndex < providerOptions.length; ++optionIndex) {
            if (providerOptions[optionIndex].id === providerId)
                return optionIndex;
        }
        return 0;
    }
    function saveSettings() {
        return settings.applyDraft({
            endpoint: endpointField.text,
            model: modelField.text,
            dictionary: dictionaryField.text,
            automaticSubtitles: automaticField.checked,
            sourceLanguage: sourceLanguage.text,
            explanationLanguage: explanationLanguage.text,
            includeSecondary: includeSecondary.checked,
            noKeyRequired: noKeyRequired.checked,
            primaryProvider: primaryProvider.currentValue,
            fallbackEnabled: fallbackEnabled.checked,
            wordProvider: wordProvider.currentValue
        });
    }
    spacing: Theme.space4
    Label {
        text: "Explanations and word lookups"
        color: Theme.ink
        font.pixelSize: Theme.title
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    ChoiceBox {
        objectName: "aiModeChoice"
        Layout.fillWidth: true
        model: ["Neden Cloud", "My own API key"]
        currentIndex: pane.account.enabled ? 0 : 1
        Accessible.name: "AI connection"
        onActivated: pane.account.enabled = currentIndex === 0
    }
    HostedAccountSettings {
        Layout.fillWidth: true
        visible: pane.account.enabled
        account: pane.account
    }
    ColumnLayout {
        objectName: "ownProviderSettings"
        visible: !pane.account.enabled
        Layout.fillWidth: true
        spacing: Theme.space3
        Label {
            text: "API address"
            color: Theme.muted
        }
        SearchField {
            id: endpointField
            objectName: "aiEndpointField"
            text: pane.settings.endpoint
            Layout.fillWidth: true
            Accessible.name: "API address"
        }
        Label {
            text: "Model"
            color: Theme.muted
        }
        SearchField {
            id: modelField
            objectName: "aiModelField"
            text: pane.settings.model
            Layout.fillWidth: true
            Accessible.name: "AI model ID"
        }
        ServiceCredentials {
            Layout.fillWidth: true
            settings: pane.settings
            service: "ai"
            prepareConnection: pane.saveSettings
        }
        SettingsDisclosure {
            objectName: "advancedAiSettings"
            title: "Advanced AI options"
            Layout.fillWidth: true
            CheckBox {
                id: noKeyRequired
                text: "Configured endpoint does not require an API key"
                checked: pane.settings.noKeyRequired
            }
            Label {
                text: "Primary explanation provider"
                color: Theme.muted
            }
            ChoiceBox {
                id: primaryProvider
                model: pane.settings.primaryProviders
                textRole: "name"
                valueRole: "id"
                currentIndex: pane.providerIndex(model, pane.settings.primaryProvider)
                Layout.fillWidth: true
                Accessible.name: "Primary explanation provider"
            }
            CheckBox {
                id: fallbackEnabled
                text: "Use the other provider after 5 seconds without an answer"
                checked: pane.settings.fallbackEnabled
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
                model: pane.settings.wordProviders
                textRole: "name"
                valueRole: "id"
                currentIndex: pane.providerIndex(model, pane.settings.wordProvider)
                Layout.fillWidth: true
                Accessible.name: "Single-click word meanings"
            }
            Label {
                text: "GPT-6 Luna key"
                color: Theme.muted
            }
            ServiceCredentials {
                Layout.fillWidth: true
                settings: pane.settings
                service: "luna"
            }
            Label {
                text: "Dictionary URL · %s is the encoded selection"
                color: Theme.muted
            }
            SearchField {
                id: dictionaryField
                objectName: "dictionaryTemplate"
                text: pane.settings.dictionaryTemplate
                Layout.fillWidth: true
                Accessible.name: "Dictionary URL template"
            }
        }
    }
    SettingsDisclosure {
        objectName: "languageSettings"
        title: "Languages and context"
        Layout.fillWidth: true
        Label {
            text: "Subtitle language"
            color: Theme.muted
        }
        SearchField {
            id: sourceLanguage
            text: pane.settings.sourceLanguage
            placeholderText: "Source language"
            Layout.fillWidth: true
            Accessible.name: "Source language"
        }
        Label {
            text: "Explain in"
            color: Theme.muted
        }
        SearchField {
            id: explanationLanguage
            text: pane.settings.explanationLanguage
            placeholderText: "Explanation language"
            Layout.fillWidth: true
            Accessible.name: "Explanation language"
        }
        CheckBox {
            id: includeSecondary
            text: "Use overlapping secondary subtitles as context"
            checked: pane.settings.includeSecondary
        }
    }
    SettingsDisclosure {
        objectName: "subtitleDownloadSettings"
        title: "Subtitle downloads"
        Layout.fillWidth: true
        CheckBox {
            id: automaticField
            checked: pane.settings.automaticSubtitles
            text: "Automatically find Turkish subtitles"
        }
        Label {
            text: "Uses your OpenSubtitles key and download allowance. Cloud does not include subtitle downloads yet. Changes apply to the next video."
            color: Theme.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ServiceCredentials {
            Layout.fillWidth: true
            settings: pane.settings
            service: "subtitles"
        }
    }
    ActionButton {
        text: "Save settings"
        objectName: "saveSettingsButton"
        onClicked: pane.saveSettings()
    }
}
