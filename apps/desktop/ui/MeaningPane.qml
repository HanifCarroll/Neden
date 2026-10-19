pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: pane
    required property LearningSession session
    readonly property StudyTools study: session.study
    readonly property bool busy: study.state === "loading" || study.state === "streaming"
    readonly property bool editing: question.activeFocus
    property bool showFullCue: false
    signal dictionaryRequested(string term)
    signal dismissed

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
        Rectangle {
            id: sourceCard
            objectName: "explanationSourceCard"
            Layout.fillWidth: true
            Layout.fillHeight: false
            implicitHeight: sourceContents.implicitHeight + Theme.space3 * 2
            radius: 10
            color: Theme.elevated
            border.color: Theme.border
            ColumnLayout {
                id: sourceContents
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space3
                spacing: Theme.space1
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    spacing: Theme.space2
                    Label {
                        objectName: "explanationSubject"
                        text: pane.study.subject || "Select a word or phrase in the subtitles."
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        wrapMode: Text.WordWrap
                        textFormat: Text.PlainText
                        color: pane.study.subject.length ? Theme.ink : Theme.muted
                        font.pixelSize: Theme.quotation
                        font.weight: Font.DemiBold
                        lineHeight: 1.4
                    }
                    IconButton {
                        objectName: "closeMeaningButton"
                        name: "x"
                        description: "Close explanation"
                        Layout.alignment: Qt.AlignTop
                        onClicked: pane.dismissed()
                    }
                }
                RowLayout {
                    Layout.fillHeight: false
                    visible: pane.study.subject.length > 0
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
                        visible: pane.study.sourceCue.trim() !== pane.study.subject.trim()
                        flat: true
                        implicitHeight: 30
                        font.pixelSize: Theme.caption
                        onClicked: pane.showFullCue = !pane.showFullCue
                    }
                }
                Label {
                    visible: pane.showFullCue && pane.study.sourceCue.trim() !== pane.study.subject.trim()
                    text: pane.study.sourceCue
                    textFormat: Text.PlainText
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.body
                    Layout.fillWidth: true
                }
            }
        }
        ListView {
            id: conversation
            objectName: "conversationList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space4
            property bool followingAnswer: true
            model: pane.study.turns.length
            onMovementStarted: followingAnswer = false
            onMovementEnded: {
                if (atYEnd)
                    followingAnswer = true;
            }
            onContentHeightChanged: {
                if (followingAnswer)
                    Qt.callLater(positionViewAtEnd);
            }
            onCountChanged: {
                followingAnswer = true;
                Qt.callLater(positionViewAtEnd);
            }
            ScrollBar.vertical: ScrollBar {}
            delegate: ColumnLayout {
                id: turn
                required property int index
                readonly property var modelData: pane.study.turns[index]
                width: conversation.width
                spacing: Theme.space2
                Label {
                    visible: turn.modelData.question.length > 0
                    text: turn.modelData.question
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    color: Theme.accent
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                Label {
                    visible: turn.modelData.provider.length > 0
                    text: turn.modelData.provider + (turn.modelData.fallback ? " · fallback" : "")
                    color: Theme.muted
                    font.pixelSize: Theme.caption
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
        Label {
            visible: pane.study.state === "failed" || pane.study.state === "incomplete" || pane.busy
            text: pane.busy ? (pane.study.state === "loading" ? "Connecting…" : "Writing…") : pane.study.message
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: pane.study.state === "failed" ? Theme.error : Theme.muted
        }
        RowLayout {
            Layout.fillHeight: false
            ActionButton {
                objectName: "explainButton"
                text: pane.study.state === "idle" ? "Explain" : "Try again"
                visible: pane.study.state === "idle" || pane.study.state === "failed" || pane.study.state === "incomplete"
                enabled: !pane.busy && (pane.session.selection.length > 0 || pane.session.subtitle.length > 0 || pane.study.subject.length > 0)
                onClicked: {
                    if (pane.study.state === "idle")
                        pane.session.explainSelection();
                    else
                        pane.study.retryExplanation();
                }
            }
            ActionButton {
                text: "Stop"
                visible: pane.busy
                onClicked: pane.study.stop()
            }
        }
        RowLayout {
            Layout.fillHeight: false
            visible: pane.study.turns.length > 0
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
                enabled: pane.study.subject.length > 0
                onClicked: pane.dictionaryRequested(pane.study.subject)
            }
            Item {
                Layout.fillWidth: true
            }
            ActionButton {
                text: "Save flashcard"
                enabled: pane.study.state === "complete"
                onClicked: pane.session.saveCard()
            }
        }
    }
    Connections {
        target: pane.study
        function onChanged() {
            if (conversation.followingAnswer)
                Qt.callLater(conversation.positionViewAtEnd);
        }
    }
}
