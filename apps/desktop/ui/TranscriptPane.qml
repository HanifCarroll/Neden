pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Rectangle {
    id: pane
    required property LearningSession session
    readonly property bool editing: search.activeFocus
    color: Theme.surface
    radius: Theme.panelRadius
    border.color: Theme.border
    function searchFor(value) {
        search.text = value;
        session.cueModel.search(value);
    }
    function toggleSearch() {
        searchFor(search.text.length ? "" : "río");
    }
    function scrollToEnd() {
        transcript.positionViewAtIndex(9500, ListView.Beginning);
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space4
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2
            RowLayout {
                Label {
                    text: "Transcript"
                    color: Theme.ink
                    font.pixelSize: Theme.title
                    font.weight: Font.DemiBold
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    text: pane.session.cueModel.count + " lines"
                    color: Theme.muted
                    font.pixelSize: Theme.caption
                }
            }
        }
        SearchField {
            id: search
            Layout.fillWidth: true
            placeholderText: "Find a word or phrase…"
            onTextChanged: pane.session.cueModel.search(text)
            Accessible.name: "Search transcript"
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Search language"
                color: Theme.muted
                font.pixelSize: Theme.caption
                Layout.fillWidth: true
            }
            ChoiceBox {
                id: language
                model: ["Turkish", "Other languages"]
                currentIndex: pane.session.cueModel.searchLanguage === CueModel.Turkish ? 0 : 1
                onActivated: pane.session.cueModel.searchLanguage = currentIndex === 0 ? CueModel.Turkish : CueModel.Unicode
                Accessible.name: "Transcript search language"
            }
        }
        ListView {
            id: transcript
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space2
            model: pane.session.cueModel
            reuseItems: true
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: cue
                required property string cueText
                required property string translation
                required property real startSeconds
                required property int cueIndex
                readonly property bool current: pane.session.activeCue === cue.cueIndex
                width: transcript.width
                height: cueContents.implicitHeight + Theme.space5
                background: Rectangle {
                    radius: 9
                    color: cue.current ? "#263b31" : (cue.hovered ? Theme.elevated : Theme.surface)
                    Rectangle {
                        width: 2
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 8
                        color: cue.current ? Theme.accent : "transparent"
                    }
                }
                Accessible.name: cue.cueText
                onClicked: pane.session.seek(startSeconds)
                ColumnLayout {
                    id: cueContents
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 7
                    Label {
                        text: Theme.clock(cue.startSeconds)
                        color: cue.current ? Theme.accent : Theme.muted
                        font.pixelSize: Theme.caption
                    }
                    Label {
                        Layout.fillWidth: true
                        text: cue.cueText
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: Theme.ink
                        font.pixelSize: Theme.body
                    }
                    Label {
                        Layout.fillWidth: true
                        text: cue.translation
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        font.pixelSize: Theme.caption
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }
        ColumnLayout {
            spacing: 2
            Label {
                text: (pane.session.study ? pane.session.study.cards.length : 0) + " flashcards saved"
                color: Theme.muted
                font.pixelSize: Theme.caption
            }
        }
    }
}
