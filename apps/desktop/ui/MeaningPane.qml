pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: pane
    required property LearningSession session
    readonly property ExplanationSession explanation: session.study.explanation
    readonly property bool busy: explanation.state === "loading" || explanation.state === "streaming"
    readonly property bool editing: question.activeFocus
    property bool showFullCue: false
    signal dictionaryRequested(string term)

    function sendQuestion() {
        if (!busy && question.text.trim().length && question.text.length <= 2000 && session.followUp(question.text)) {
            question.text = "";
            question.forceActiveFocus();
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3
        Item {
            id: sourceCard
            objectName: "explanationSourceCard"
            Layout.fillWidth: true
            Layout.fillHeight: false
            implicitHeight: sourceContents.implicitHeight + Theme.space3
            ColumnLayout {
                id: sourceContents
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: Theme.space1
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    spacing: Theme.space2
                    Label {
                        objectName: "explanationSubject"
                        text: pane.explanation.subject || "Select a word or phrase in the subtitles."
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                        color: pane.explanation.subject.length ? Theme.ink : Theme.muted
                        font.pixelSize: Theme.quotation
                        font.weight: Font.DemiBold
                        lineHeight: 1.4
                    }
                }
                RowLayout {
                    Layout.fillHeight: false
                    visible: pane.explanation.subject.length > 0
                    ActionButton {
                        text: pane.session.replaying ? "Stop replay" : "Replay line"
                        flat: true
                        implicitHeight: 30
                        font.pixelSize: Theme.caption
                        onClicked: pane.session.replayExplainedCue()
                    }
                    ActionButton {
                        objectName: "fullSubtitleButton"
                        text: "Full subtitle line"
                        visible: pane.explanation.sourceCue.trim() !== pane.explanation.subject.trim()
                        flat: true
                        implicitHeight: 30
                        font.pixelSize: Theme.caption
                        onClicked: pane.showFullCue = !pane.showFullCue
                    }
                }
                Label {
                    visible: pane.showFullCue && pane.explanation.sourceCue.trim() !== pane.explanation.subject.trim()
                    text: pane.explanation.sourceCue
                    textFormat: Text.PlainText
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.body
                    Layout.fillWidth: true
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }
        ListView {
            id: conversation
            objectName: "conversationList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space4
            model: pane.explanation.turns.length
            function anchorLatestReply() {
                forceLayout();
                if (count > 0)
                    positionViewAtIndex(count - 1, ListView.Beginning);
                else
                    positionViewAtBeginning();
            }
            onCountChanged: Qt.callLater(anchorLatestReply)
            Component.onCompleted: Qt.callLater(anchorLatestReply)
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                id: turn
                required property int index
                readonly property var modelData: pane.explanation.turns[index]
                width: conversation.width
                // Reserve space below a new question so it can start at the top before
                // any answer arrives. Streaming expands downward without moving the viewport.
                height: Math.max(turnContents.implicitHeight, index === conversation.count - 1 ? conversation.height : 0)
                ColumnLayout {
                    id: turnContents
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: Theme.space2
                    Label {
                        objectName: "conversationQuestion"
                        visible: turn.modelData.question.length > 0
                        text: turn.modelData.question
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: Theme.accent
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    TextArea {
                        objectName: "explanationAnswerText"
                        Layout.preferredHeight: contentHeight
                        Layout.minimumHeight: contentHeight
                        text: ExplanationFormat.render(turn.modelData.answer)
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.RichText
                        color: Theme.ink
                        font.pixelSize: Theme.body
                        Layout.fillWidth: true
                        padding: 0
                        background: Item {}
                    }
                    Label {
                        visible: turn.modelData.status === "failed" || turn.modelData.status === "incomplete"
                        text: turn.modelData.status === "incomplete" ? "Incomplete answer" : "Couldn’t get an answer"
                        textFormat: Text.PlainText
                        color: Theme.error
                        Layout.fillWidth: true
                    }
                }
            }
        }
        Label {
            visible: pane.explanation.state === "failed" || pane.explanation.state === "incomplete" || pane.busy
            text: pane.busy ? (pane.explanation.state === "loading" ? "Connecting…" : "Writing…") : pane.explanation.message
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: pane.explanation.state === "failed" ? Theme.error : Theme.muted
        }
        RowLayout {
            Layout.fillHeight: false
            ActionButton {
                objectName: "explainButton"
                text: pane.explanation.state === "idle" ? "Explain" : "Try again"
                visible: pane.explanation.state === "idle" || pane.explanation.state === "failed" || pane.explanation.state === "incomplete"
                enabled: !pane.busy && (pane.session.selection.length > 0 || pane.session.subtitle.length > 0 || pane.explanation.subject.length > 0)
                onClicked: {
                    if (pane.explanation.state === "idle")
                        pane.session.explainSelection();
                    else
                        pane.explanation.retryExplanation();
                }
            }
            ActionButton {
                text: "Stop"
                visible: pane.busy
                onClicked: pane.explanation.stop()
            }
        }
        RowLayout {
            Layout.fillHeight: false
            visible: pane.explanation.turns.length > 0
            Layout.fillWidth: true
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(100, Math.max(54, question.implicitHeight))
                TextArea {
                    id: question
                    objectName: "followUpInput"
                    placeholderText: "Ask a follow-up…"
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    font.pixelSize: Theme.body
                    color: Theme.ink
                    placeholderTextColor: Theme.muted
                    Accessible.name: "Follow-up question"
                    background: Rectangle {
                        color: Theme.background
                        border.color: question.activeFocus ? Theme.accent : Theme.border
                        radius: Theme.controlRadius
                    }
                    Keys.onReturnPressed: event => {
                        if (!(event.modifiers & Qt.ShiftModifier)) {
                            pane.sendQuestion();
                            event.accepted = true;
                        } else
                            event.accepted = false;
                    }
                    Keys.onEnterPressed: event => {
                        if (!(event.modifiers & Qt.ShiftModifier)) {
                            pane.sendQuestion();
                            event.accepted = true;
                        } else
                            event.accepted = false;
                    }
                }
            }
            ActionButton {
                objectName: "sendFollowUpButton"
                text: "Send"
                enabled: !pane.busy && question.text.trim().length > 0 && question.text.length <= 2000
                onClicked: pane.sendQuestion()
            }
        }
        Label {
            visible: question.text.length > 2000
            text: "Keep the question within 2,000 characters."
            color: Theme.error
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillHeight: false
            Layout.fillWidth: true
            ActionButton {
                text: "Dictionary"
                flat: true
                enabled: pane.explanation.subject.length > 0
                onClicked: pane.dictionaryRequested(pane.explanation.subject)
            }
            Item {
                Layout.fillWidth: true
            }
            ActionButton {
                text: "Save flashcard"
                enabled: pane.explanation.state === "complete"
                onClicked: pane.session.saveCard()
            }
        }
    }
    Connections {
        target: pane.explanation
        function onReplyStarted() {
            Qt.callLater(conversation.anchorLatestReply);
        }
    }
}
