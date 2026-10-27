pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ScrollView {
    id: pane
    required property LearningSession session
    readonly property SubtitleTools tools: session.study.subtitleTools
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: pane.availableWidth
        spacing: Theme.space5
        SettingsGroup {
            title: "Translate this track"
            description: "Start from accurate subtitles. Timing stays unchanged; the translated copy loads in the other slot. AI translation can make mistakes."
            Layout.fillWidth: true
            RowLayout {
                Layout.fillWidth: true
                ChoiceBox {
                    id: language
                    objectName: "trackTranslationLanguage"
                    Layout.fillWidth: true
                    textRole: "name"
                    model: pane.session.study.settings.explanationLanguages
                    currentIndex: Math.max(0, model.findIndex(item => item.name === pane.session.study.settings.explanationLanguage))
                    Accessible.name: "Translate subtitles into"
                }
                ActionButton {
                    objectName: "translateTrackButton"
                    text: "Translate…"
                    enabled: !pane.tools.busy && pane.tools.lineCount > 0
                    onClicked: translationConfirmation.open()
                }
            }
        }
        SettingsGroup {
            title: "Repair timing"
            description: "Use an accurate track from the same cut in the other slot. Alignment checks the whole track for an offset and common frame-rate drift."
            Layout.fillWidth: true
            ActionButton {
                objectName: "alignTrackButton"
                text: "Align to the other track"
                enabled: !pane.tools.busy && pane.tools.lineCount > 0
                onClicked: pane.tools.alignToReference()
            }
        }
        SettingsDisclosure {
            title: "Match lines manually"
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space3
                Label {
                    text: "Enter when a line should start in the video, in seconds. A second match at least 30 seconds later also corrects drift."
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    Layout.fillWidth: true
                }
                RowLayout {
                    Label { text: "First line"; color: Theme.ink }
                    SpinBox { id: firstLine; from: 1; to: Math.max(1, pane.tools.lineCount); editable: true; Accessible.name: "First matching subtitle line" }
                    TextField { id: firstTime; placeholderText: "Video seconds"; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly; Accessible.name: "First matching video time in seconds" }
                    ActionButton { text: "Use current"; onClicked: firstTime.text = pane.session.position.toFixed(3) }
                }
                Label { text: pane.tools.lineText(firstLine.value - 1); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                CheckBox { id: correctDrift; text: "Correct drift with a second match" }
                RowLayout {
                    visible: correctDrift.checked
                    Label { text: "Later line"; color: Theme.ink }
                    SpinBox { id: secondLine; from: 1; to: Math.max(1, pane.tools.lineCount); editable: true; Accessible.name: "Second matching subtitle line" }
                    TextField { id: secondTime; placeholderText: "Video seconds"; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly; Accessible.name: "Second matching video time in seconds" }
                    ActionButton { text: "Use current"; onClicked: secondTime.text = pane.session.position.toFixed(3) }
                }
                Label { visible: correctDrift.checked; text: pane.tools.lineText(secondLine.value - 1); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ActionButton {
                    text: "Preview correction"
                    enabled: !pane.tools.busy && pane.tools.lineCount > 0 && firstTime.text.trim().length > 0 && (!correctDrift.checked || secondTime.text.trim().length > 0)
                    onClicked: pane.tools.matchLines(firstLine.value - 1, Number(firstTime.text), correctDrift.checked ? secondLine.value - 1 : -1, correctDrift.checked ? Number(secondTime.text) : -1)
                }
            }
        }
        Label {
            objectName: "subtitleToolsStatus"
            text: pane.tools.message
            wrapMode: Text.WordWrap
            color: Theme.ink
            Layout.fillWidth: true
            Accessible.role: Accessible.StaticText
        }
        RowLayout {
            ActionButton { objectName: "applyTimingRepairButton"; text: "Apply correction"; visible: pane.tools.hasRepair; onClicked: pane.tools.applyRepair() }
            ActionButton { text: "Cancel"; visible: pane.tools.busy; onClicked: pane.tools.cancel() }
        }
    }
    Popup {
        id: translationConfirmation
        objectName: "trackTranslationConfirmation"
        anchors.centerIn: parent
        width: Math.min(Math.max(280, pane.width - 20), 500)
        padding: Theme.space5
        implicitHeight: confirmationContent.implicitHeight + topPadding + bottomPadding
        modal: true
        focus: true
        contentItem: ColumnLayout {
            id: confirmationContent
            spacing: Theme.space4
            Label {
                text: "Translate this subtitle track?"
                font.pixelSize: Theme.title
                color: Theme.ink
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: pane.tools.lineCount + " lines will be sent to " + (pane.session.study.account.enabled ? "Neden Cloud" : "your configured AI provider") + ". This uses about " + pane.tools.translationBatches + (pane.session.study.account.enabled ? " explanation requests from your allowance." : " AI requests, billed by your provider.") + " A separate copy will replace the other slot only when the full translation succeeds."
                wrapMode: Text.WordWrap
                color: Theme.ink
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                ActionButton { text: "Cancel"; onClicked: translationConfirmation.close() }
                ActionButton {
                    text: "Translate"
                    onClicked: {
                        translationConfirmation.close();
                        pane.tools.translate(language.currentText);
                    }
                }
            }
        }
        background: Rectangle { color: Theme.surface; border.color: Theme.border; radius: Theme.controlRadius }
    }
}
