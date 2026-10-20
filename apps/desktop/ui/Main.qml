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
    enum InspectorMode {
        Hidden,
        Transcript,
        Meaning,
        Dictionary
    }
    property int inspectorMode: Main.Transcript
    readonly property bool dictionaryOpen: inspectorMode === Main.Dictionary
    readonly property bool transcriptOpen: inspectorMode !== Main.Hidden
    readonly property int inspectorTab: inspectorMode === Main.Meaning ? 1 : 0
    property bool showRecent: false
    readonly property bool meaningEditing: transcriptOpen && inspectorTab === 1 && meaningLoader.item !== null && (meaningLoader.item as MeaningPane).editing
    width: 1280
    height: 756
    minimumWidth: 900
    minimumHeight: 540
    visible: true
    flags: Qt.platform.os === "osx" ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint : Qt.Window
    topPadding: 0
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

    Connections {
        target: root.appearance
        function onChanged() {
            if (!root.appearance.selectable) {
                root.session.invalidateStudy();
            }
        }
    }
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
                onTriggered: root.inspectorMode = checked ? Main.Transcript : Main.Hidden
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
        function onExplanationRequested() {
            root.openMeaning();
        }
        function onMediaChanged() {
            root.showRecent = false;
        }
    }
    function openMeaning() {
        inspectorMode = Main.Meaning;
    }
    function toggleTranscript() {
        if (transcriptOpen && inspectorTab === 0 && !dictionaryOpen) {
            inspectorMode = Main.Hidden;
        } else {
            closeSelection();
            inspectorMode = Main.Transcript;
        }
    }
    function openStudy(tab) {
        studyLoader.active = true;
        const pane = studyLoader.item as StudyPane;
        pane.currentTab = tab;
        studyDrawer.open();
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
        inspectorMode = Main.Dictionary;
        if (studyDrawer.opened)
            studyDrawer.close();
        const pane = dictionaryLoader.item as DictionaryPane;
        pane.lookup(term || session.selection);
    }
    function closeDictionary() {
        inspectorMode = Main.Transcript;
        closeSelection();
    }
    function escapeLayer() {
        if (settingsDialog.opened)
            settingsDialog.close();
        else if (studyDrawer.opened)
            studyDrawer.close();
        else if (dictionaryOpen)
            closeDictionary();
        else if (session.study.words.state !== "idle")
            session.closeWordCard();
        else if (session.selection.length) {
            closeSelection();
            inspectorMode = Main.Transcript;
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
    }
    Shortcut {
        sequence: "F11"
        onActivated: root.fullscreen()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: root.escapeLayer()
    }
    PlayerShortcuts {
        session: root.session
        available: !root.showRecent && !settingsDialog.opened && !studyDrawer.opened && !root.dictionaryOpen
        textEntryActive: root.meaningEditing || transcript.editing
        controlKeysActive: playbackControls.editing
        subtitleSelectionActive: stage.editing
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
        nameFilters: ["Subtitle files (*.srt *.vtt)"]
        onAccepted: root.session.openSubtitles(selectedFile)
    }

    FileDialog {
        id: translationDialog
        objectName: "secondaryPicker"
        title: "Choose Secondary subtitles"
        nameFilters: ["Subtitle files (*.srt *.vtt)"]
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
        onClosed: {
            root.appearance.cancelPreview();
            settingsLoader.active = false;
            root.contentItem.forceActiveFocus();
        }
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
            root.session.study.explanation.stop();
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

    StackLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
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
                    onFullscreenRequested: root.fullscreen()
                    windowDragEnabled: Qt.platform.os === "osx" && !root.fullscreenController.active
                    onMoveWindowRequested: root.startSystemMove()
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
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            spacing: 0
                            Layout.topMargin: Math.max(root.fullscreenController.safeTopInset, root.contentItem.SafeArea.margins.top)
                            InspectorTab {
                                objectName: "transcriptTab"
                                text: "Transcript"
                                Layout.fillWidth: true
                                checked: root.inspectorTab === 0 && !root.dictionaryOpen
                                onClicked: {
                                    root.closeSelection();
                                    root.inspectorMode = Main.Transcript;
                                }
                            }
                            InspectorTab {
                                objectName: "meaningTab"
                                text: "Meaning"
                                Layout.fillWidth: true
                                checked: root.inspectorTab === 1 && !root.dictionaryOpen
                                onClicked: root.openMeaning()
                            }
                            IconButton {
                                name: "x"
                                description: "Close sidebar"
                                objectName: "closeSidebarButton"
                                onClicked: {
                                    root.closeSelection();
                                    root.inspectorMode = Main.Hidden;
                                }
                            }
                        }
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
                                    onDismissed: {
                                        root.closeSelection();
                                        root.inspectorMode = Main.Hidden;
                                    }
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
                    }
                }
            }
            PlaybackControls {
                id: playbackControls
                Layout.fillWidth: true
                session: root.session
                sidebarOpen: root.transcriptOpen || root.dictionaryOpen
                onToggleSidebar: {
                    if (sidebarOpen) {
                        root.closeSelection();
                        root.inspectorMode = Main.Hidden;
                    } else
                        root.toggleTranscript();
                }
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
