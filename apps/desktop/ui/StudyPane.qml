pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Neden

Rectangle {
    id: pane
    required property LearningSession session
    readonly property StudyTools study: session.study
    property int revealedCard: -1
    signal dismissed
    color: Theme.background

    FileDialog {
        id: exportDialog
        title: "Export flashcards for Anki"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Tab-separated text (*.tsv)"]
        defaultSuffix: "tsv"
        onAccepted: pane.study.exportCards(selectedFile)
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16
        RowLayout {
            Label { text: "Study"; font.pixelSize: 24; color: Theme.ink; Layout.fillWidth: true }
            ActionButton { text: "Close"; onClicked: pane.dismissed() }
        }
        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: "Learn" }
            TabButton { text: "Cards" }
            TabButton { text: "Subtitles" }
        }
        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                Label {
                    text: pane.session.selection.length ? pane.session.selection : pane.session.subtitle
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    color: Theme.accent
                    font.pixelSize: 18
                }
                Label {
                    text: "The selected phrase and current subtitle are sent to your configured AI provider when you request help."
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted
                }
                RowLayout {
                    ActionButton { text: "Explain"; onClicked: pane.session.explainSelection(false) }
                    ActionButton { text: "Translate"; onClicked: pane.session.explainSelection(true) }
                    ActionButton { text: "Stop"; enabled: pane.study.state === "loading" || pane.study.state === "streaming"; onClicked: pane.study.stop() }
                }
                Label { text: pane.study.state; color: Theme.muted }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    TextArea {
                        text: ExplanationFormat.render(pane.study.answer)
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.RichText
                        color: Theme.ink
                    }
                }
                ActionButton { text: "Save flashcard"; onClicked: pane.session.saveCard() }
            }
            ColumnLayout {
                RowLayout {
                    Label { text: pane.study.cards.length + " cards on this device"; Layout.fillWidth: true; color: Theme.muted }
                    ActionButton { text: "Export to Anki"; onClicked: exportDialog.open() }
                }
                ListView {
                    id: cardList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 12
                    model: pane.study.cards
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        required property int index
                        width: cardList.width
                        height: cardContents.implicitHeight + 28
                        color: Theme.surface
                        radius: 12
                        ColumnLayout {
                            id: cardContents
                            width: parent.width - 28
                            x: 14; y: 14
                            Label { text: card.modelData.front; color: Theme.accent; wrapMode: Text.WordWrap; textFormat: Text.PlainText; Layout.fillWidth: true }
                            Label { text: "Due " + card.modelData.due; color: Theme.muted; font.pixelSize: 11 }
                            Label { visible: pane.revealedCard === card.index; text: card.modelData.back; color: Theme.ink; wrapMode: Text.WordWrap; textFormat: Text.PlainText; Layout.fillWidth: true }
                            RowLayout {
                                ActionButton { text: "Reveal"; onClicked: pane.revealedCard = card.index }
                                ActionButton { text: "Again"; enabled: pane.revealedCard === card.index; onClicked: { pane.study.reviewCard(card.index, false); pane.revealedCard = -1; } }
                                ActionButton { text: "Remembered"; enabled: pane.revealedCard === card.index; onClicked: { pane.study.reviewCard(card.index, true); pane.revealedCard = -1; } }
                            }
                            ActionButton { text: "Remove"; flat: true; onClicked: { pane.study.removeCard(card.index); pane.revealedCard = -1; } }
                        }
                    }
                }
            }
            ColumnLayout {
                Label { text: "Search by fingerprint, or enter a title and season/episode (0 means any). Check the release and timing before using a match."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                SearchField { id: subtitleTitle; placeholderText: "Title (optional; empty uses video fingerprint)"; Accessible.name: "Subtitle search title"; Layout.fillWidth: true }
                RowLayout {
                    Label { text: "Season"; color: Theme.muted }
                    SpinBox { id: subtitleSeason; from: 0; to: 1000; editable: true; Accessible.name: "Subtitle season" }
                    Label { text: "Episode"; color: Theme.muted }
                    SpinBox { id: subtitleEpisode; from: 0; to: 10000; editable: true; Accessible.name: "Subtitle episode" }
                }
                RowLayout {
                    ActionButton { text: "Find Turkish"; onClicked: pane.study.findSubtitles("tr", false, subtitleTitle.text, subtitleSeason.value, subtitleEpisode.value) }
                    ActionButton { text: "Find English"; onClicked: pane.study.findSubtitles("en", false, subtitleTitle.text, subtitleSeason.value, subtitleEpisode.value) }
                }
                ListView {
                    id: resultsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: pane.study.subtitleResults
                    delegate: ItemDelegate {
                        id: result
                        required property var modelData
                        required property int index
                        width: resultsList.width
                        text: (modelData.exact ? "Hash match · " : "Check release · ") + modelData.release
                        contentItem: Label {
                            text: result.text
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: Theme.ink
                        }
                        onClicked: pane.study.downloadSubtitle(result.index)
                    }
                }
                Label { text: "Choosing a result downloads and loads the source track. Load a second local SRT using Load subtitles → Translation subtitles."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }

        }
        Label { text: pane.study.message; Layout.fillWidth: true; wrapMode: Text.WordWrap; textFormat: Text.PlainText; color: Theme.muted }
    }
}
