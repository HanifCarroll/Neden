pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Neden

ApplicationWindow {
    id: root
    property ThumbnailService thumbnails: null
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
    property bool appendToQueue: false
    property bool screenshotWithSubtitles: true
    property bool floatingMode: false
    property bool controlsIdle: false
    readonly property bool hidePlaybackControls: fullscreenController.active && controlsIdle && !session.paused && !transcriptOpen && !dictionaryOpen
    function revealPlaybackControls() {
        controlsIdle = false;
        controlsIdleTimer.restart();
    }
    HoverHandler {
        blocking: false
        property point lastPointerPosition: Qt.point(-1, -1)
        onPointChanged: {
            // Qt can deliver stationary hover updates while the video renders.
            // Only actual pointer movement should restart the inactivity timer.
            if (!hovered)
                return;
            const position = point.position;
            if (position.x !== lastPointerPosition.x || position.y !== lastPointerPosition.y) {
                lastPointerPosition = position;
                root.revealPlaybackControls();
            }
        }
    }
    Timer {
        id: controlsIdleTimer
        interval: 2500
        onTriggered: {
            if (!playbackControls.hovered && !playbackControls.interacting)
                root.controlsIdle = true;
            else
                restart();
        }
    }
    property size normalSize: Qt.size(1280, 756)
    property point normalPosition: Qt.point(0, 0)
    function openPlayerOptions(page) {
        playerOptions.page = page;
        playerOptionsDialog.open();
    }
    function toggleFloating() {
        if (fullscreenController.active)
            fullscreenController.setActive(false);
        floatingMode = !floatingMode;
        if (floatingMode) {
            normalSize = Qt.size(width, height);
            normalPosition = Qt.point(x, y);
            inspectorMode = Main.Hidden;
            flags = flags | Qt.WindowStaysOnTopHint;
            width = 640;
            height = 390;
        } else {
            flags = flags & ~Qt.WindowStaysOnTopHint;
            width = normalSize.width;
            height = normalSize.height;
            x = normalPosition.x;
            y = normalPosition.y;
        }
        show();
    }
    readonly property bool meaningEditing: transcriptOpen && inspectorTab === 1 && meaningLoader.item !== null && (meaningLoader.item as MeaningPane).editing
    width: 1280
    height: 756
    minimumWidth: floatingMode ? 480 : 900
    minimumHeight: floatingMode ? 300 : 540
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
                text: "Open folder…"
                onTriggered: folderDialog.open()
            }
            Action {
                text: "Open URL…"
                shortcut: "Ctrl+Shift+O"
                onTriggered: urlDialog.open()
            }
            Action {
                text: "Add files to queue…"
                onTriggered: {
                    root.appendToQueue = true;
                    videoDialog.open();
                }
            }
            Action {
                text: "Show file in folder"
                enabled: root.session.hasMedia
                onTriggered: root.session.revealFile()
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
                text: "Previous video"
                shortcut: "Ctrl+Shift+Left"
                enabled: root.session.queue.hasPrevious
                onTriggered: root.session.queue.previous()
            }
            Action {
                text: "Next video"
                shortcut: "Ctrl+Shift+Right"
                enabled: root.session.queue.hasNext
                onTriggered: root.session.queue.next()
            }
            Action {
                text: "Queue…"
                onTriggered: root.openPlayerOptions(PlayerOptions.Queue)
            }
            Action {
                text: "Start over"
                enabled: root.session.hasMedia
                onTriggered: root.session.startOver()
            }
            Action {
                text: "Jump to time…"
                shortcut: "Ctrl+J"
                enabled: root.session.seekable
                onTriggered: timeDialog.open()
            }
            Action {
                text: "Undo last seek"
                enabled: root.session.seekable
                onTriggered: root.session.undoSeek()
            }
            Menu {
                id: chapterMenu
                title: "Chapters"
                enabled: root.session.chapters.length > 0
                Action {
                    text: "Previous chapter"
                    onTriggered: root.session.chapterStep(-1)
                }
                Action {
                    text: "Next chapter"
                    onTriggered: root.session.chapterStep(1)
                }
                MenuSeparator {}
                Instantiator {
                    model: root.session.chapters
                    delegate: MenuItem {
                        required property int index
                        required property var modelData
                        text: Theme.clock(modelData.time) + "  " + (modelData.title || "Chapter " + (index + 1))
                        onTriggered: root.session.jumpToChapter(index)
                    }
                    onObjectAdded: (index, object) => chapterMenu.insertItem(index + 3, object)
                    onObjectRemoved: (index, object) => chapterMenu.removeItem(object)
                }
            }
            Action {
                text: "Previous frame"
                enabled: root.session.seekable
                onTriggered: root.session.frameStep(false)
            }
            Action {
                text: "Next frame"
                enabled: root.session.seekable
                onTriggered: root.session.frameStep(true)
            }
            Action {
                text: root.session.loopLabel
                enabled: root.session.seekable
                onTriggered: root.session.cycleLoop()
            }
            Action {
                text: "Slower by 0.05×"
                onTriggered: root.session.rate(root.session.playbackSpeed - 0.05)
            }
            Action {
                text: "Faster by 0.05×"
                onTriggered: root.session.rate(root.session.playbackSpeed + 0.05)
            }
            Action {
                text: "Reset playback speed"
                onTriggered: root.session.rate(1)
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
            title: "Audio"
            Action {
                text: "Tracks and output…"
                onTriggered: root.openPlayerOptions(PlayerOptions.Audio)
            }
            Action {
                text: "Audio earlier by 0.1 s"
                onTriggered: root.session.preferences.audioDelay -= 0.1
            }
            Action {
                text: "Audio later by 0.1 s"
                onTriggered: root.session.preferences.audioDelay += 0.1
            }
            Action {
                text: "Reset audio delay"
                onTriggered: root.session.preferences.audioDelay = 0
            }
        }
        Menu {
            title: "Video"
            Action {
                text: "Picture options…"
                onTriggered: root.openPlayerOptions(PlayerOptions.Picture)
            }
            Action {
                text: "Save screenshot with subtitles…"
                enabled: root.session.hasMedia
                onTriggered: {
                    root.screenshotWithSubtitles = true;
                    screenshotDialog.open();
                }
            }
            Action {
                text: "Save clean screenshot…"
                enabled: root.session.hasMedia
                onTriggered: {
                    root.screenshotWithSubtitles = false;
                    screenshotDialog.open();
                }
            }
            Action {
                text: "Media information…"
                enabled: root.session.hasMedia
                onTriggered: root.openPlayerOptions(PlayerOptions.Information)
            }
            Action {
                text: "Floating player"
                checkable: true
                checked: root.floatingMode
                onTriggered: root.toggleFloating()
            }
        }
        Menu {
            title: "Subtitles"
            Action {
                text: "Tracks, timing and encoding…"
                onTriggered: root.openPlayerOptions(PlayerOptions.Subtitles)
            }
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
                text: Qt.platform.os === "osx" ? "macOS fullscreen" : "Fullscreen"
                shortcut: "Ctrl+Meta+F"
                onTriggered: root.fullscreenController.toggleNative()
            }
            MenuSeparator {}
            Action {
                text: "Playback preferences…"
                onTriggered: {
                    root.settingsPage = 1;
                    root.openSettings();
                }
            }
            MenuItem {
                text: "Settings…"
                visible: Qt.platform.os !== "osx"
                onTriggered: root.openSettings()
            }
        }
    }
    Connections {
        target: root.session
        function onPausedChanged() {
            root.revealPlaybackControls();
        }
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
    property int settingsPage: 0
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
        function onActiveChanged() {
            root.revealPlaybackControls();
        }
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
        onInstantFullscreenRequested: root.fullscreenController.toggleInstant()
        available: !root.showRecent && !settingsDialog.opened && !studyDrawer.opened && !playerOptionsDialog.opened && !timeDialog.opened && !urlDialog.opened && !root.dictionaryOpen
        textEntryActive: root.meaningEditing || transcript.editing
        controlKeysActive: playbackControls.editing
        subtitleSelectionActive: stage.editing
    }
    FileDialog {
        id: videoDialog
        objectName: "videoPicker"
        title: root.appendToQueue ? "Add videos to queue" : "Open videos"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Video files (*.mp4 *.mkv *.webm *.mov *.m4v *.avi *.mpg *.mpeg *.ts *.wmv)", "All files (*)"]
        onAccepted: {
            if (root.appendToQueue)
                root.session.queue.append(selectedFiles);
            else
                root.session.openVideos(selectedFiles);
            root.appendToQueue = false;
        }
        onRejected: root.appendToQueue = false
    }
    FileDialog {
        id: subtitleDialog
        objectName: "primaryPicker"
        title: "Choose Primary subtitles"
        nameFilters: ["Subtitle files (*.srt *.vtt *.ass *.ssa *.sup *.sub *.idx)"]
        onAccepted: root.session.openSubtitles(selectedFile)
    }

    FileDialog {
        id: translationDialog
        objectName: "secondaryPicker"
        title: "Choose Secondary subtitles"
        nameFilters: ["Subtitle files (*.srt *.vtt *.ass *.ssa *.sup *.sub *.idx)"]
        onAccepted: root.session.openTranslation(selectedFile)
    }
    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            if (drop.hasUrls) {
                root.session.acceptDrop(drop.urls);
                drop.acceptProposedAction();
            }
        }
    }
    FileDialog {
        id: locateDialog
        property url oldUrl
        title: "Locate moved video"
        onAccepted: root.recentVideos.relocate(oldUrl, selectedFile)
    }
    FolderDialog {
        id: folderDialog
        title: "Open video folder"
        onAccepted: root.session.openVideos([selectedFolder])
    }
    FileDialog {
        id: screenshotDialog
        title: "Save screenshot"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG image (*.png)"]
        defaultSuffix: "png"
        onAccepted: {
            if (root.screenshotWithSubtitles)
                stage.saveScreenshot(root.session.localFilePath(selectedFile));
            else
                root.session.saveScreenshot(selectedFile);
        }
    }
    OptionsDialog {
        id: playerOptionsDialog
        objectName: "playerOptionsDialog"
        title: "Playback options"
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 40, 820)
        height: Math.min(root.height - 40, 660)
        contentItem: PlayerOptions {
            id: playerOptions
            objectName: "playerOptions"
            onLoadSubtitles: secondary => secondary ? translationDialog.open() : subtitleDialog.open()
            session: root.session
            onAddFiles: {
                root.appendToQueue = true;
                videoDialog.open();
            }
        }
        onClosed: root.contentItem.forceActiveFocus()
    }
    Dialog {
        id: timeDialog
        title: "Jump to time"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Cancel
        ColumnLayout {
            SearchField {
                id: timeField
                placeholderText: "HH:MM:SS or seconds"
                Accessible.name: "Jump to time"
                onAccepted: jumpButton.clicked()
            }
            Label {
                id: timeError
                color: Theme.muted
                text: ""
            }
            ActionButton {
                id: jumpButton
                text: "Go"
                onClicked: {
                    if (root.session.jumpToTime(timeField.text))
                        timeDialog.close();
                    else
                        timeError.text = "Enter a valid playback time.";
                }
            }
        }
        onOpened: timeField.forceActiveFocus()
        onClosed: root.contentItem.forceActiveFocus()
    }
    Dialog {
        id: urlDialog
        title: "Open media URL"
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 40, 560)
        standardButtons: Dialog.Cancel
        ColumnLayout {
            width: parent.width
            SearchField {
                id: urlField
                Layout.fillWidth: true
                placeholderText: "https://…"
                Accessible.name: "Media URL"
            }
            Label {
                text: "Use a direct HTTP or HTTPS media URL."
                color: Theme.muted
            }
            ActionButton {
                text: "Open"
                onClicked: {
                    root.session.openVideos([urlField.text]);
                    urlDialog.close();
                }
            }
        }
        onClosed: root.contentItem.forceActiveFocus()
    }
    OptionsDialog {
        id: settingsDialog
        objectName: "settingsDialog"
        title: "Settings"
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 720)
        height: Math.min(root.height - 48, 700)
        modal: true
        contentItem: Loader {
            id: settingsLoader
            objectName: "settingsLoader"
            active: false
            // Retain drafts after first use, without constructing controls at startup.
            sourceComponent: SettingsPane {
                page: root.settingsPage
                onPageChanged: root.settingsPage = page
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
            onLocateVideo: oldUrl => {
                locateDialog.oldUrl = oldUrl;
                locateDialog.open();
            }
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
                    hideCursor: root.hidePlaybackControls
                    onScreenshotFinished: saved => {
                        screenshotResult.text = saved ? "Screenshot saved" : "The screenshot could not be saved. Choose a writable folder.";
                        screenshotResult.open();
                    }
                    onFullscreenRequested: root.fullscreen()
                    onVideoClicked: {
                        if (root.session.paused && root.transcriptOpen) {
                            root.session.closeWordCard(false);
                            root.closeSelection();
                            root.inspectorMode = Main.Hidden;
                            root.session.setPlaying();
                        }
                    }
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
                thumbnails: root.thumbnails
                opacity: root.hidePlaybackControls ? 0 : 1
                enabled: !root.hidePlaybackControls
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
    ToolTip {
        id: screenshotResult
        x: Math.max(16, (root.width - width) / 2)
        y: root.height - height - 90
        timeout: 4000
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
