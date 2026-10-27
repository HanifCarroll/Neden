pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Neden

Rectangle {
    id: pane
    required property LearningSession session
    readonly property StudyServices studyServices: session.study
    property int revealedCard: -1
    property alias currentTab: tabs.currentIndex
    readonly property bool busy: studyServices.explanation.state === "loading" || studyServices.explanation.state === "streaming"
    signal dismissed
    color: Theme.background

    FileDialog {
        id: exportDialog
        title: "Export flashcards for Anki"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Tab-separated text (*.tsv)"]
        defaultSuffix: "tsv"
        onAccepted: pane.studyServices.deck.exportCards(selectedFile)
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space5
        spacing: Theme.space4
        RowLayout {
            Label {
                text: "Study"
                font.pixelSize: Theme.heading
                color: Theme.ink
                Layout.fillWidth: true
            }
            ActionButton {
                text: "Close"
                onClicked: pane.dismissed()
            }
        }
        SegmentBar {
            id: tabs
            Layout.fillWidth: true
            SegmentButton {
                width: tabs.availableWidth / Math.max(1, tabs.count)
                text: "Learn"
            }
            SegmentButton {
                width: tabs.availableWidth / Math.max(1, tabs.count)
                text: "Cards"
            }
            SegmentButton {
                width: tabs.availableWidth / Math.max(1, tabs.count)
                text: "Subtitles"
            }
        }
        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true
            ExplanationPane {
                session: pane.session
                onFlashcardsRequested: tabs.currentIndex = 1
            }
            ColumnLayout {
                RowLayout {
                    Label {
                        text: pane.studyServices.deck.cards.length + " cards on this device"
                        Layout.fillWidth: true
                        color: Theme.muted
                    }
                    ActionButton {
                        text: "Export to Anki"
                        onClicked: exportDialog.open()
                    }
                }
                ListView {
                    id: cardList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Theme.space3
                    model: pane.studyServices.deck.cards
                    delegate: Item {
                        id: card
                        required property var modelData
                        required property int index
                        width: cardList.width
                        height: cardContents.implicitHeight + Theme.space5
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }
                        ColumnLayout {
                            id: cardContents
                            width: parent.width
                            Label {
                                text: card.modelData.front
                                color: Theme.accent
                                wrapMode: Text.WordWrap
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                            }
                            Label {
                                text: "Due " + card.modelData.due
                                color: Theme.muted
                                font.pixelSize: Theme.caption
                            }
                            Label {
                                visible: pane.revealedCard === card.index
                                text: card.modelData.back
                                color: Theme.ink
                                wrapMode: Text.WordWrap
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                            }
                            RowLayout {
                                ActionButton {
                                    text: "Reveal"
                                    onClicked: pane.revealedCard = card.index
                                }
                                ActionButton {
                                    text: "Again"
                                    enabled: pane.revealedCard === card.index
                                    onClicked: {
                                        pane.studyServices.deck.reviewCard(card.index, false);
                                        pane.revealedCard = -1;
                                    }
                                }
                                ActionButton {
                                    text: "Remembered"
                                    enabled: pane.revealedCard === card.index
                                    onClicked: {
                                        pane.studyServices.deck.reviewCard(card.index, true);
                                        pane.revealedCard = -1;
                                    }
                                }
                            }
                            ActionButton {
                                text: "Remove"
                                flat: true
                                onClicked: {
                                    pane.studyServices.deck.removeCard(card.index);
                                    pane.revealedCard = -1;
                                }
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                ChoiceBox {
                    id: subtitleProvider
                    objectName: "subtitleSearchProviderChoice"
                    Layout.fillWidth: true
                    model: [{id: "subvault", name: "Subvault · TV episodes"}, {id: "subdl", name: "SubDL · Movies and TV"}, {id: "opensubtitles", name: "OpenSubtitles · Fingerprint search"}]
                    textRole: "name"
                    valueRole: "id"
                    Accessible.name: "Subtitle source"
                    onActivated: pane.studyServices.subtitles.clearSearch()
                }
                Label {
                    text: subtitleProvider.currentValue === "opensubtitles" ? "Search by fingerprint, or enter a title and season/episode (0 means any)." : "Enter the title and choose a matching release. For TV, include the season and episode."
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                SearchField {
                    id: subtitleTitle
                    objectName: "subtitleSearchTitle"
                    placeholderText: subtitleProvider.currentValue === "opensubtitles" ? "Title (optional; empty uses video fingerprint)" : "Movie or show title"
                    Accessible.name: "Subtitle search title"
                    Layout.fillWidth: true
                }
                RowLayout {
                    Label {
                        text: "Season"
                        color: Theme.muted
                    }
                    SpinBox {
                        id: subtitleSeason
                        objectName: "subtitleSearchSeason"
                        from: 0
                        to: 1000
                        editable: true
                        Accessible.name: "Subtitle season"
                    }
                    Label {
                        text: "Episode"
                        color: Theme.muted
                    }
                    SpinBox {
                        id: subtitleEpisode
                        objectName: "subtitleSearchEpisode"
                        from: 0
                        to: 10000
                        editable: true
                        Accessible.name: "Subtitle episode"
                    }
                }
                RowLayout {
                    ChoiceBox {
                        id: subtitleLanguage
                        objectName: "subtitleSearchLanguageChoice"
                        Layout.fillWidth: true
                        model: pane.studyServices.settings.subtitleLanguages
                        textRole: "name"
                        valueRole: "id"
                        currentIndex: {
                            const preferred = pane.studyServices.settings.subtitleLanguageCode || pane.studyServices.settings.subtitleCodeForLanguage(pane.studyServices.settings.explanationLanguage);
                            for (let index = 0; index < model.length; ++index) {
                                if (model[index].id === preferred)
                                    return index;
                            }
                            return -1;
                        }
                        displayText: currentIndex < 0 ? "Choose language…" : currentText
                        Accessible.name: "Subtitle search language"
                        onActivated: pane.studyServices.subtitles.clearSearch()
                    }
                    ActionButton {
                        text: "Find subtitles"
                        objectName: "subtitleSearchButton"
                        enabled: subtitleLanguage.currentIndex >= 0
                        onClicked: pane.studyServices.subtitles.findSubtitles(subtitleLanguage.currentValue, false, subtitleTitle.text, subtitleSeason.value, subtitleEpisode.value, subtitleProvider.currentValue)
                    }
                }
                ListView {
                    id: resultsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: pane.studyServices.subtitles.subtitleResults
                    delegate: ItemDelegate {
                        id: result
                        required property var modelData
                        required property int index
                        width: resultsList.width
                        text: (modelData.kind === "title" ? "Choose title · " : modelData.exact ? "Hash match · " : "Load release · ") + modelData.release + (modelData.year > 0 ? " (" + modelData.year + ")" : "")
                        contentItem: Label {
                            text: result.text
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: Theme.ink
                        }
                        onClicked: pane.studyServices.subtitles.downloadSubtitle(result.index)
                    }
                }
                Label {
                    text: "Choose a title, then a release to load as the primary track. Adjust timing with the subtitle controls."
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        Label {
            visible: tabs.currentIndex !== 0
            text: tabs.currentIndex === 1 ? pane.studyServices.deck.message : pane.studyServices.subtitles.message
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: Theme.muted
        }
    }
}
