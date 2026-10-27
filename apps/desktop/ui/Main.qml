pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ApplicationWindow {
    id: root
    readonly property MotionPreferences motionPreferences: MotionPreferences {}
    property ThumbnailService thumbnails: null
    property ShortcutMap keymap: ShortcutMap {}
    readonly property bool shortcutsBlocked: keymap.recording || dictionaryOpen || dialogs.blocksShortcuts || studyDrawer.opened
    readonly property bool textEntryActive: explanationEditing || transcript.editing
    readonly property bool commandShortcutsEnabled: !shortcutsBlocked && !textEntryActive
    required property SubtitleAppearance appearance
    required property LearningSession session
    required property RecentVideos recentVideos
    required property MpvPlayback playback
    required property FullscreenController fullscreenController
    enum InspectorMode {
        Hidden,
        Transcript,
        Explanation,
        Dictionary
    }
    property int inspectorMode: Main.Transcript
    readonly property bool dictionaryOpen: inspectorMode === Main.Dictionary
    readonly property bool transcriptOpen: inspectorMode !== Main.Hidden
    readonly property int inspectorTab: inspectorMode === Main.Explanation ? 1 : 0
    property bool showRecent: false
    property bool floatingMode: false
    property bool controlsIdle: false
    readonly property bool hidePlaybackControls: session.hasMedia && !showRecent && controlsIdle && commandShortcutsEnabled && !stage.selecting
    onShowRecentChanged: revealPlaybackControls()
    onActiveChanged: revealPlaybackControls()
    function revealPlaybackControls() {
        controlsIdle = false;
        controlsIdleTimer.restart();
    }
    PlaybackPointer {
        objectName: "playbackPointer"
        window: root
        videoArea: stage
        hideRequested: root.hidePlaybackControls && !stage.wordCardOpen
        onActivity: root.revealPlaybackControls()
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
    readonly property bool explanationEditing: transcriptOpen && inspectorTab === 1 && explanationLoader.item !== null && (explanationLoader.item as ExplanationPane).editing
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
    menuBar: PlayerMenuBar {
        session: root.session
        keymap: root.keymap
        fullscreenController: root.fullscreenController
        commandShortcutsEnabled: root.commandShortcutsEnabled
        floatingMode: root.floatingMode
        transcriptOpen: root.transcriptOpen
        onGettingStartedRequested: dialogs.openGuide()
        onSupportRequested: updates => dialogs.openSupport(updates)
        onOpenVideoRequested: append => dialogs.openVideo(append)
        onOpenFolderRequested: dialogs.openFolder()
        onOpenUrlRequested: dialogs.openUrl()
        onJumpToTimeRequested: dialogs.jumpToTime()
        onPrimarySubtitlesRequested: dialogs.openPrimarySubtitles()
        onSecondarySubtitlesRequested: dialogs.openSecondarySubtitles()
        onScreenshotRequested: withSubtitles => dialogs.saveScreenshot(withSubtitles)
        onPlayerOptionsRequested: page => dialogs.openPlayerOptions(page)
        onStudyRequested: tab => root.openStudy(tab)
        onSettingsRequested: page => root.openSettings(page)
        onDictionaryRequested: root.openDictionary("")
        onExplanationRequested: root.openExplanation()
        onFloatingRequested: root.toggleFloating()
        onCloseRequested: root.close()
        onRecentVideosRequested: root.showRecent = true
        onSidebarRequested: root.inspectorMode = root.transcriptOpen ? Main.Hidden : Main.Transcript
    }
    Connections {
        target: root.session
        function onPausedChanged() {
            root.revealPlaybackControls();
        }
        function onExplanationRequested() {
            root.openExplanation();
        }
        function onMediaChanged() {
            root.revealPlaybackControls();
            root.showRecent = false;
        }
    }
    function openExplanation() {
        inspectorMode = Main.Explanation;
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
    function openSettings(page) {
        if (studyDrawer.opened)
            studyDrawer.close();
        dialogs.openSettings(page);
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
        if (dialogs.settingsOpen)
            dialogs.closeSettings();
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
        bindings: root.keymap.bindings
        onExplainSubtitleRequested: root.session.explainCue(root.session.subtitle.length === 0)
        onInstantFullscreenRequested: root.fullscreenController.toggleInstant()
        available: !root.shortcutsBlocked && !root.showRecent
        textEntryActive: root.textEntryActive
        controlKeysActive: playbackControls.editing
        subtitleSelectionActive: stage.editing
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
    PlayerDialogs {
        id: dialogs
        anchors.fill: parent
        session: root.session
        recentVideos: root.recentVideos
        keymap: root.keymap
        appearance: root.appearance
        fullscreenController: root.fullscreenController
        onFocusRequested: root.contentItem.forceActiveFocus()
        onSubtitleScreenshotRequested: path => stage.saveScreenshot(path)
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
            onOpenVideo: dialogs.openVideo()
            onGettingStarted: dialogs.openGuide()
            onLocateVideo: oldUrl => dialogs.locateVideo(oldUrl)
        }
        Item {
            Item {
                clip: true
                anchors.fill: parent
                anchors.bottomMargin: root.fullscreenController.active ? 0 : playbackControls.height
                VideoStage {
                    id: stage
                    anchors.fill: parent
                    anchors.rightMargin: inspector.reservedWidth
                    session: root.session
                    playback: root.playback
                    appearance: root.appearance
                    bottomOverlayHeight: root.fullscreenController.active && !root.hidePlaybackControls ? playbackControls.height : 0
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
                        } else {
                            root.session.toggle();
                        }
                    }
                    windowDragEnabled: Qt.platform.os === "osx" && !root.fullscreenController.active
                    onMoveWindowRequested: root.startSystemMove()
                }
                SlidingSidebar {
                    id: inspector
                    objectName: "learningInspector"
                    color: Theme.surface
                    expanded: root.dictionaryOpen || root.transcriptOpen
                    reduceMotion: root.motionPreferences.reducedMotion
                    width: Math.max(280, Math.min(352, root.width * 0.275))
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
                                objectName: "explanationTab"
                                text: "Explanation"
                                Layout.fillWidth: true
                                checked: root.inspectorTab === 1 && !root.dictionaryOpen
                                onClicked: root.openExplanation()
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
                                onPrimaryRequested: dialogs.openPrimarySubtitles()
                                onSecondaryRequested: dialogs.openSecondarySubtitles()
                            }
                            Loader {
                                id: explanationLoader
                                objectName: "explanationLoader"
                                active: root.inspectorTab === 1
                                sourceComponent: ExplanationPane {
                                    session: root.session
                                    onFlashcardsRequested: root.openStudy(1)
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
                objectName: "playbackControls"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: root.fullscreenController.active ? inspector.reservedWidth : 0
                z: 1
                thumbnails: root.thumbnails
                opacity: root.hidePlaybackControls ? 0 : 1
                enabled: !root.hidePlaybackControls
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
