pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ScrollView {
    id: guide
    signal openVideoRequested
    signal subtitlesRequested
    signal servicesRequested
    contentWidth: availableWidth
    ColumnLayout {
        width: guide.availableWidth
        spacing: Theme.space5
        Label {
            text: "Watch something you enjoy. Explore the words as you go."
            color: Theme.ink
            font.pixelSize: Theme.title
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: "1. Open your video"
            font.bold: true
            color: Theme.ink
        }
        Label {
            text: "Use a video on your computer. Neden remembers your place. Videos and subtitles are supplied by you."
            color: Theme.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ActionButton {
            text: "Open video…"
            primary: true
            onClicked: guide.openVideoRequested()
        }
        Label {
            text: "2. Choose your subtitles"
            font.bold: true
            color: Theme.ink
        }
        Label {
            text: "Choose the language you're learning as Primary in the Transcript sidebar. Add a translation as Secondary if helpful. You can use embedded tracks or load subtitle files."
            color: Theme.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ActionButton {
            text: "Subtitle tracks…"
            onClicked: guide.subtitlesRequested()
        }
        Label {
            text: "3. Connect AI when you're ready"
            font.bold: true
            color: Theme.ink
        }
        Label {
            text: "Playback works without an account. For word meanings and explanations, choose Neden Cloud or your own API key in Settings → AI & accounts. Cloud is an invited preview; your own provider bills API usage separately. Relevant subtitle text and questions are sent to your chosen service. Keys are stored securely on your device."
            color: Theme.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ActionButton {
            text: "Connect AI…"
            onClicked: guide.servicesRequested()
        }
        Label {
            text: "Click a subtitle word for its meaning. Use Explain line for a full breakdown, then ask a follow-up. Save flashcard keeps the line in your local collection, available from Study → Flashcards. Customize keyboard controls in Settings → Shortcuts."
            color: Theme.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
