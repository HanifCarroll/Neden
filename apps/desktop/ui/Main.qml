pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Neden

ApplicationWindow {
    id: root
    required property SubtitleAppearance appearance
    required property LearningSession session
    required property RecentVideos recentVideos
    required property MpvPlayback playback
    required property FullscreenController fullscreenController
    property bool dictionaryOpen: false
    property bool showRecent: false
    property bool transcriptOpen: true
    property string requestedSelection: ""
    width: 1280
    height: 820
    minimumWidth: 1100
    minimumHeight: 640
    visible: true
    title: session.hasMedia ? session.mediaTitle + " — Neden" : "Neden"
    onClosing: session.checkpoint()
    color: Theme.background
    font.family: "Inter"
    font.pixelSize: Theme.body
    palette.window: Theme.background
    palette.base: Theme.surface
    palette.text: Theme.ink
    palette.button: Theme.elevated
    palette.buttonText: Theme.ink
    palette.windowText: Theme.ink
    palette.light: Theme.elevated
    palette.midlight: Theme.elevated
    palette.dark: Theme.border
    palette.mid: Theme.border
    palette.shadow: Theme.background
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.background

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action {
                text: "Open video…"
                shortcut: StandardKey.Open
                onTriggered: videoDialog.open()
            }
            Action {
                text: "Recently played"
                onTriggered: root.showRecent = true
            }
            MenuSeparator {}
            Action {
                text: "Close window"
                shortcut: StandardKey.Close
                onTriggered: root.close()
            }
            Action {
                text: "Quit Neden"
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: "Playback"
            Action {
                text: root.session.paused ? "Play" : "Pause"
                enabled: root.session.hasMedia
                onTriggered: root.session.toggle()
            }
            Action {
                text: "Replay sentence"
                enabled: root.session.activeCue >= 0
                onTriggered: root.session.replay()
            }
            MenuSeparator {}
            Action {
                text: "Seek forward 5 seconds"
                enabled: root.session.hasMedia
                onTriggered: root.session.seek(root.session.position + 5)
            }
            Action {
                text: "Seek back 5 seconds"
                enabled: root.session.hasMedia
                onTriggered: root.session.seek(root.session.position - 5)
            }
            MenuSeparator {}
            Action {
                text: root.session.volumeLevel === 0 ? "Unmute" : "Mute"
                onTriggered: root.session.toggleMute()
            }
            Action {
                text: "Volume up"
                shortcut: "Ctrl+Up"
                onTriggered: root.session.volume(root.session.volumeLevel + 5)
            }
            Action {
                text: "Volume down"
                shortcut: "Ctrl+Down"
                onTriggered: root.session.volume(root.session.volumeLevel - 5)
            }
        }
        Menu {
            title: "Subtitles"
            Action {
                text: "Load primary track…"
                enabled: root.session.hasMedia
                onTriggered: subtitleDialog.open()
            }
            Action {
                text: "Load secondary track…"
                enabled: root.session.hasMedia
                onTriggered: translationDialog.open()
            }
            MenuSeparator {}
            Action {
                text: "Find subtitles…"
                enabled: root.session.hasMedia
                onTriggered: root.openStudy(2)
            }
            Action {
                text: "Subtitle appearance…"
                onTriggered: root.openSettings()
            }
        }
        Menu {
            title: "Study"
            Action {
                text: "Explain selection"
                enabled: root.session.selection.length > 0 || root.session.subtitle.length > 0
                onTriggered: {
                    root.openStudy(0);
                    root.session.explainSelection(false);
                }
            }
            Action {
                text: "Translate selection"
                enabled: root.session.selection.length > 0 || root.session.subtitle.length > 0
                onTriggered: {
                    root.openStudy(0);
                    root.session.explainSelection(true);
                }
            }
            Action {
                text: "Dictionary"
                shortcut: "Ctrl+D"
                onTriggered: root.openDictionary("")
            }
            MenuSeparator {}
            Action {
                text: "Flashcards"
                onTriggered: root.openStudy(1)
            }
        }
        Menu {
            title: "View"
            Action {
                text: "Transcript"
                checkable: true
                checked: root.transcriptOpen
                onTriggered: root.transcriptOpen = checked
            }
            Action {
                text: "Recently played"
                shortcut: "Ctrl+L"
                onTriggered: root.showRecent = true
            }
            Action {
                text: "Fullscreen"
                shortcut: "Ctrl+Meta+F"
                onTriggered: root.fullscreen()
            }
            MenuSeparator {}
            Action {
                text: "Settings…"
                onTriggered: root.openSettings()
            }
        }
    }
    Connections {
        target: root.session
        function onSelectionChanged() {
            root.requestedSelection = "";
        }
        function onMediaChanged() {
            root.showRecent = false;
        }
    }
    function openStudy(tab) {
        studyLoader.active = true;
        const pane = studyLoader.item as StudyPane;
        pane.currentTab = tab;
        studyDrawer.open();
    }
    function explainCommittedSelection() {
        if (!session.selection.length || requestedSelection === session.selection)
            return;

        requestedSelection = session.selection;
        openStudy(0);
        session.explainSelection(false);
    }
    function openSettings() {
        if (studyDrawer.opened)
            studyDrawer.close();
        settingsLoader.active = true;
        settingsDialog.open();
    }
    function fullscreen() {
        fullscreenController.toggle();
    }
    function closeSelection() {
        session.closeDefinition();
        stage.deselect();
        contentItem.forceActiveFocus();
    }
    function openDictionary(term) {
        dictionaryOpen = true;
        if (studyDrawer.opened)
            studyDrawer.close();
        const pane = dictionaryLoader.item as DictionaryPane;
        pane.lookup(term || session.selection);
    }
    function closeDictionary() {
        dictionaryOpen = false;
        closeSelection();
    }
    function escapeLayer() {
        if (settingsDialog.opened)
            settingsDialog.close();
        else if (studyDrawer.opened)
            studyDrawer.close();
        else if (dictionaryOpen)
            closeDictionary();
        else if (session.selection.length)
            closeSelection();
        else if (session.replaying)
            session.stopReplay();
        else if (fullscreenController.active)
            fullscreenController.setActive(false);
    }
    Shortcut {
        sequence: "Ctrl+," // Qt maps Control to Command on macOS.
        onActivated: root.openSettings()
    }
    Connections {
        target: root.fullscreenController
        function onSettingsRequested() {
            root.openSettings();
        }
    }
    Shortcut {
        sequence: "F11"
        onActivated: root.fullscreen()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: root.escapeLayer()
    }
    Shortcut {
        sequence: "Space"
        enabled: root.session.hasMedia && !root.showRecent && !settingsDialog.opened && !studyDrawer.opened && !root.dictionaryOpen && !transcript.editing && !stage.editing && !playbackControls.editing
        onActivated: root.session.toggle()
    }
    Shortcut {
        sequence: "Right"
        enabled: root.session.hasMedia && !root.showRecent && !settingsDialog.opened && !studyDrawer.opened && !root.dictionaryOpen && !transcript.editing && !stage.editing && !playbackControls.editing
        onActivated: root.session.seek(root.session.position + 5)
    }
    Shortcut {
        sequence: "Left"
        enabled: root.session.hasMedia && !root.showRecent && !settingsDialog.opened && !studyDrawer.opened && !root.dictionaryOpen && !transcript.editing && !stage.editing && !playbackControls.editing
        onActivated: root.session.seek(root.session.position - 5)
    }
    FileDialog {
        id: videoDialog
        objectName: "videoPicker"
        title: "Open local video"
        nameFilters: ["Video files (*.mp4 *.mkv *.webm *.mov *.m4v *.avi *.mpg *.mpeg *.ts *.wmv)", "All files (*)"]
        onAccepted: root.session.openVideo(selectedFile)
    }
    FileDialog {
        id: subtitleDialog
        objectName: "primaryPicker"
        title: "Choose Primary subtitles"
        nameFilters: ["SRT subtitles (*.srt)"]
        onAccepted: root.session.openSubtitles(selectedFile)
    }

    FileDialog {
        id: translationDialog
        objectName: "secondaryPicker"
        title: "Choose Secondary subtitles"
        nameFilters: ["SRT subtitles (*.srt)"]
        onAccepted: root.session.openTranslation(selectedFile)
    }
    Dialog {
        id: settingsDialog
        objectName: "settingsDialog"
        title: "Settings"
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 640)
        height: Math.min(root.height - 48, 700)
        modal: true
        padding: Theme.space5
        background: Rectangle {
            color: Theme.background
            border.color: Theme.border
            radius: Theme.panelRadius
        }
        header: Label {
            text: "Settings"
            color: Theme.ink
            font.pixelSize: Theme.heading
            font.weight: Font.DemiBold
            padding: Theme.space5
        }
        footer: Item {
            implicitHeight: Theme.controlHeight + Theme.space5
            ActionButton {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space5
                text: "Close"
                onClicked: settingsDialog.close()
            }
        }
        contentItem: Loader {
            id: settingsLoader
            objectName: "settingsLoader"
            active: false
            // Retain drafts after first use, without constructing controls at startup.
            sourceComponent: SettingsPane {
                session: root.session
                fullscreenController: root.fullscreenController
                appearance: root.appearance
            }
        }
        onClosed: root.contentItem.forceActiveFocus()
    }
    Drawer {
        id: studyDrawer
        objectName: "studyDrawer"
        edge: Qt.RightEdge
        width: Math.min(root.width - 40, 530)
        height: root.height
        modal: true
        onAboutToShow: studyLoader.active = true
        onClosed: {
            root.session.study.stop();
            if (!root.dictionaryOpen)
                root.closeSelection();
        }
        Loader {
            id: studyLoader
            objectName: "studyLoader"
            anchors.fill: parent
            active: false
            sourceComponent: StudyPane {
                session: root.session
                onDismissed: studyDrawer.close()
                onDictionaryRequested: term => root.openDictionary(term)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space5
        anchors.rightMargin: Theme.space5
        anchors.topMargin: 8
        anchors.bottomMargin: 16
        spacing: 0
        PlayerHeader {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            hasMedia: root.session.hasMedia
            showingRecent: !root.session.hasMedia || root.showRecent
            onOpenRecent: root.showRecent = true
            onBackToVideo: root.showRecent = false
            onOpenVideo: videoDialog.open()
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: 24
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 1
                color: Theme.border
            }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.session.hasMedia && !root.showRecent ? 1 : 0
            RecentVideosPane {
                recentVideos: root.recentVideos
                session: root.session
                onOpenVideo: videoDialog.open()
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.space5
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0
                    Item {
                        implicitHeight: 8
                    }
                    RowLayout {
                        Label {
                            text: root.session.mediaTitle || "Your listening space"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            color: Theme.ink
                            font.pixelSize: 18
                            font.weight: Font.Medium
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                    Item {
                        implicitHeight: 18
                    }
                    VideoStage {
                        id: stage
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        session: root.session
                        playback: root.playback
                        appearance: root.appearance
                        onExplainSelection: root.explainCommittedSelection()
                        dictionaryOpen: root.dictionaryOpen
                        onOpenDictionary: root.openDictionary("")
                        onCloseSelection: root.closeSelection()
                    }
                    PlaybackControls {
                        id: playbackControls
                        Layout.fillWidth: true
                        session: root.session
                        onFullscreen: root.fullscreen()
                    }
                    Item {
                        implicitHeight: 16
                    }
                    Label {
                        text: "Double-click a word or select a phrase for an explanation."
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                }
                TranscriptPane {
                    id: transcript
                    visible: !root.dictionaryOpen && root.transcriptOpen
                    Layout.preferredWidth: 352
                    Layout.fillHeight: true
                    session: root.session
                }
                Loader {
                    id: dictionaryLoader
                    objectName: "dictionaryLoader"
                    active: root.dictionaryOpen
                    visible: active
                    Layout.preferredWidth: 352
                    Layout.fillHeight: true
                    sourceComponent: DictionaryPane {
                        session: root.session
                        onDismissed: root.closeDictionary()
                    }
                }
            }
        }
        Item {
            implicitHeight: 18
        }
        RowLayout {
            Rectangle {
                implicitWidth: 6
                implicitHeight: 6
                radius: 3
                color: Theme.accent
            }
            Label {
                text: root.recentVideos.error || root.session.status
                color: Theme.muted
                font.pixelSize: 11
            }
            Item {
                Layout.fillWidth: true
            }
        }
    }
}
