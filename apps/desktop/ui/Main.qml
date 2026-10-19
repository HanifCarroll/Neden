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
    property int inspectorTab: 0
    property string requestedSelection: ""
    width: 1280
    height: 756
    minimumWidth: 900
    minimumHeight: 540
    visible: true
    title: session.hasMedia && !showRecent ? session.mediaTitle + " — Neden" : "Neden"
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
                text: "Previous subtitle"
                enabled: root.session.canPreviousSubtitle
                shortcut: "Ctrl+Left"
                onTriggered: root.session.previousSubtitle()
            }
            Action {
                text: "Next subtitle"
                enabled: root.session.canNextSubtitle
                shortcut: "Ctrl+Right"
                onTriggered: root.session.nextSubtitle()
            }
            Action {
                text: "Replay subtitle"
                shortcut: "Ctrl+R"
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
                    root.openMeaning();
                    root.session.explainSelection();
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
            MenuItem {
                text: "Settings…"
                visible: Qt.platform.os !== "osx"
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
    function openMeaning() {
        dictionaryOpen = false;
        transcriptOpen = true;
        inspectorTab = 1;
    }
    function toggleTranscript() {
        if (transcriptOpen && inspectorTab === 0 && !dictionaryOpen) {
            transcriptOpen = false;
        } else {
            closeSelection();
            dictionaryOpen = false;
            transcriptOpen = true;
            inspectorTab = 0;
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
        openMeaning();
        session.explainSelection();
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
        else if (session.selection.length) {
            closeSelection();
            inspectorTab = 0;
        } else if (session.replaying)
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
        function onTranscriptRequested() {
            root.toggleTranscript();
        }
        function onMeaningRequested() {
            root.openMeaning();
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
        sequence: "M"
        enabled: root.session.hasMedia && !root.showRecent && !settingsDialog.opened && !studyDrawer.opened && !root.dictionaryOpen && !transcript.editing && !stage.editing && !playbackControls.editing
        onActivated: root.session.toggleMute()
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

    header: ToolBar {
        visible: Qt.platform.os !== "osx" || root.fullscreenController.active
        implicitHeight: visible ? 36 + root.fullscreenController.safeTopInset : 0
        background: Rectangle {
            color: Theme.surface
        }
        RowLayout {
            anchors.fill: parent
            anchors.topMargin: root.fullscreenController.safeTopInset
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            Label {
                text: root.session.hasMedia ? root.session.mediaTitle : "Neden"
                color: Theme.muted
                font.pixelSize: Theme.caption
                textFormat: Text.PlainText
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            IconButton {
                name: "captions"
                description: "Toggle transcript"
                onClicked: root.toggleTranscript()
            }
            IconButton {
                name: "info"
                description: "Show meaning"
                onClicked: root.openMeaning()
            }
        }
    }
    StackLayout {
        anchors.fill: parent
        currentIndex: root.session.hasMedia && !root.showRecent ? 1 : 0
        RecentVideosPane {
            Layout.margins: Theme.space5
            recentVideos: root.recentVideos
            session: root.session
            onOpenVideo: videoDialog.open()
        }
        ColumnLayout {
            spacing: 0
            RowLayout {
                spacing: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                VideoStage {
                    id: stage
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    session: root.session
                    playback: root.playback
                    appearance: root.appearance
                    onExplainSelection: root.explainCommittedSelection()
                }
                Rectangle {
                    objectName: "learningInspector"
                    color: Theme.surface
                    visible: root.dictionaryOpen || root.transcriptOpen
                    Layout.preferredWidth: Math.max(280, Math.min(352, root.width * 0.275))
                    Layout.fillHeight: true
                    Rectangle {
                        width: 1
                        height: parent.height
                        color: Theme.border
                    }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 1
                        spacing: 0
                        StackLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            currentIndex: root.dictionaryOpen ? 2 : root.inspectorTab
                            TranscriptPane {
                                id: transcript
                                session: root.session
                                onPrimaryRequested: subtitleDialog.open()
                                onSecondaryRequested: translationDialog.open()
                            }
                            Loader {
                                id: meaningLoader
                                objectName: "meaningLoader"
                                active: root.inspectorTab === 1
                                sourceComponent: MeaningPane {
                                    session: root.session
                                    onDictionaryRequested: term => root.openDictionary(term)
                                }
                            }
                            Loader {
                                id: dictionaryLoader
                                objectName: "dictionaryLoader"
                                active: root.dictionaryOpen
                                sourceComponent: DictionaryPane {
                                    session: root.session
                                    onDismissed: root.closeDictionary()
                                }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Theme.border
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            spacing: 0
                            InspectorTab {
                                objectName: "transcriptTab"
                                text: "Transcript"
                                Layout.fillWidth: true
                                checked: root.inspectorTab === 0 && !root.dictionaryOpen
                                onClicked: {
                                    root.closeSelection();
                                    root.dictionaryOpen = false;
                                    root.inspectorTab = 0;
                                }
                            }
                            InspectorTab {
                                objectName: "meaningTab"
                                text: "Meaning"
                                Layout.fillWidth: true
                                checked: root.inspectorTab === 1 && !root.dictionaryOpen
                                onClicked: root.openMeaning()
                            }
                        }
                    }
                }
            }
            PlaybackControls {
                id: playbackControls
                Layout.fillWidth: true
                session: root.session
                onFullscreen: root.fullscreen()
            }
        }
    }
    Popup {
        id: errorNotice
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 480)
        padding: Theme.space4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: Theme.elevated
            radius: Theme.controlRadius
            border.color: Theme.border
        }
        contentItem: Label {
            text: root.recentVideos.error || root.session.status
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            color: Theme.ink
        }
    }
    Connections {
        target: root.session
        function onStatusChanged() {
            const normal = ["Ready to explore", "Loading local video…", "Primary subtitles loaded", "Secondary subtitles loaded"];
            if (!normal.includes(root.session.status))
                errorNotice.open();
        }
    }
    Connections {
        target: root.recentVideos
        function onErrorChanged() {
            if (root.recentVideos.error.length)
                errorNotice.open();
        }
    }
}
