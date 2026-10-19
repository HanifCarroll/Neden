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
    property alias currentTab: tabs.currentIndex
    readonly property bool busy: study.state === "loading" || study.state === "streaming"
    signal dictionaryRequested(string term)
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
        TabBar {
            id: tabs
            Layout.fillWidth: true
            SegmentButton {
                text: "Learn"
            }
            SegmentButton {
                text: "Cards"
            }
            SegmentButton {
                text: "Subtitles"
            }
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
                    font.pixelSize: Theme.title
                }
                Label {
                    text: "The selected phrase and current subtitle are sent to your configured AI provider when you request help."
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                }
                RowLayout {
                    ActionButton {
                        objectName: "explainButton"
                        text: pane.study.state === "failed" || pane.study.state === "incomplete" ? "Try again" : "Explain"
                        enabled: !pane.busy
                        onClicked: pane.session.explainSelection(false)
                    }
                    ActionButton {
                        text: "Translate"
                        enabled: !pane.busy
                        onClicked: pane.session.explainSelection(true)
                    }
                    ActionButton {
                        text: "Stop"
                        enabled: pane.study.state === "loading" || pane.study.state === "streaming"
                        onClicked: pane.study.stop()
                    }
                }
                ActionButton {
                    text: "Look up in dictionary"
                    flat: true
                    enabled: pane.session.selection.length > 0 || pane.session.subtitle.length > 0
                    onClicked: pane.dictionaryRequested(pane.session.selection || pane.session.subtitle)
                }
                Label {
                    visible: pane.study.state !== "idle"
                    text: ({
                            loading: "Connecting…",
                            streaming: "Writing…",
                            complete: "Complete",
                            incomplete: "Incomplete answer",
                            failed: "Couldn’t get an explanation"
                        })[pane.study.state] || ""
                    color: pane.study.state === "failed" ? Theme.error : Theme.muted
                }
                Label {
                    visible: pane.study.state === "failed" || pane.study.state === "incomplete"
                    text: pane.study.message
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                    color: Theme.muted
                }
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
                        font.pixelSize: Theme.body
                        padding: 0
                        background: Item {}
                    }
                }
                ActionButton {
                    text: "Save flashcard"
                    enabled: pane.study.state === "complete"
                    onClicked: pane.session.saveCard()
                }
            }
            ColumnLayout {
                RowLayout {
                    Label {
                        text: pane.study.cards.length + " cards on this device"
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
                    model: pane.study.cards
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        required property int index
                        width: cardList.width
                        height: cardContents.implicitHeight + 28
                        color: Theme.surface
                        radius: Theme.panelRadius
                        ColumnLayout {
                            id: cardContents
                            width: parent.width - 28
                            x: 14
                            y: 14
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
                                        pane.study.reviewCard(card.index, false);
                                        pane.revealedCard = -1;
                                    }
                                }
                                ActionButton {
                                    text: "Remembered"
                                    enabled: pane.revealedCard === card.index
                                    onClicked: {
                                        pane.study.reviewCard(card.index, true);
                                        pane.revealedCard = -1;
                                    }
                                }
                            }
                            ActionButton {
                                text: "Remove"
                                flat: true
                                onClicked: {
                                    pane.study.removeCard(card.index);
                                    pane.revealedCard = -1;
                                }
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                Label {
                    text: "Search by fingerprint, or enter a title and season/episode (0 means any). Check the release and timing before using a match."
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                SearchField {
                    id: subtitleTitle
                    placeholderText: "Title (optional; empty uses video fingerprint)"
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
                        from: 0
                        to: 10000
                        editable: true
                        Accessible.name: "Subtitle episode"
                    }
                }
                RowLayout {
                    ActionButton {
                        text: "Find Turkish"
                        onClicked: pane.study.findSubtitles("tr", false, subtitleTitle.text, subtitleSeason.value, subtitleEpisode.value)
                    }
                    ActionButton {
                        text: "Find English"
                        onClicked: pane.study.findSubtitles("en", false, subtitleTitle.text, subtitleSeason.value, subtitleEpisode.value)
                    }
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
                Label {
                    text: "Choosing a result downloads and loads the source track. Load a second local SRT using Subtitles → Load secondary track."
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        Label {
            visible: tabs.currentIndex !== 0
            text: pane.study.message
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: Theme.muted
        }
    }
}
