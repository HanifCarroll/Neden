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
    signal dictionaryRequested(string term)
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3
        Label {
            objectName: "explanationSubject"
            text: pane.study.subject || "Select a word or phrase in the subtitles."
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            maximumLineCount: 4
            elide: Text.ElideRight
            color: Theme.accent
            font.pixelSize: Theme.title
        }
        Label {
            text: "Explanations use your configured AI provider and the current subtitle for context."
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.muted
        }
        RowLayout {
            ActionButton {
                objectName: "explainButton"
                text: pane.study.state === "failed" || pane.study.state === "incomplete" ? "Try again" : "Explain"
                visible: pane.study.state === "idle" || pane.study.state === "failed" || pane.study.state === "incomplete"
                enabled: !pane.busy && (pane.session.selection.length > 0 || pane.session.subtitle.length > 0)
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
                enabled: pane.study.state === "loading" || pane.study.state === "streaming"
                onClicked: pane.study.stop()
            }
        }
        ActionButton {
            text: "Look up in dictionary"
            flat: true
            enabled: pane.session.selection.length > 0 || pane.session.subtitle.length > 0
            onClicked: pane.dictionaryRequested(pane.study.subject || pane.session.selection || pane.session.subtitle)
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
}
