pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Neden

MenuBar {
    id: playerMenu
    objectName: "playerMenuBar"
    required property LearningSession session
    required property ShortcutMap keymap
    required property FullscreenController fullscreenController
    property bool commandShortcutsEnabled: false
    property bool floatingMode: false
    property bool transcriptOpen: false
    signal openVideoRequested(bool append)
    signal openFolderRequested
    signal openUrlRequested
    signal jumpToTimeRequested
    signal primarySubtitlesRequested
    signal secondarySubtitlesRequested
    signal screenshotRequested(bool withSubtitles)
    signal playerOptionsRequested(int page)
    signal studyRequested(int tab)
    signal settingsRequested(int page)
    signal dictionaryRequested
    signal explanationRequested
    signal floatingRequested
    signal closeRequested
    signal recentVideosRequested
    signal gettingStartedRequested
    signal supportRequested(bool updates)
    signal sidebarRequested

    Menu {
        title: "&File"
        MenuAction {
            text: "Open video…"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.openVideo : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.openVideoRequested(false)
        }
        Action {
            text: "Open folder…"
            onTriggered: playerMenu.openFolderRequested()
        }
        MenuAction {
            text: "Open URL…"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.openUrl : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.openUrlRequested()
        }
        Action {
            text: "Add files to queue…"
            onTriggered: {
                playerMenu.openVideoRequested(true);
            }
        }
        Action {
            text: "Show file in folder"
            enabled: playerMenu.session.hasMedia
            onTriggered: playerMenu.session.revealFile()
        }
        Action {
            text: "Recently played"
            onTriggered: playerMenu.recentVideosRequested()
        }
        MenuSeparator {}
        Action {
            text: "Close window"
            shortcut: StandardKey.Close
            onTriggered: playerMenu.closeRequested()
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
            text: playerMenu.session.paused ? "Play" : "Pause"
            enabled: playerMenu.session.hasMedia
            onTriggered: playerMenu.session.toggle()
        }
        MenuAction {
            text: "Previous subtitle"
            enabled: playerMenu.session.canPreviousSubtitle
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.previousSubtitle : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.session.previousSubtitle()
        }
        MenuAction {
            text: "Next subtitle"
            enabled: playerMenu.session.canNextSubtitle
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.nextSubtitle : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.session.nextSubtitle()
        }
        MenuAction {
            text: "Replay subtitle"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.replaySubtitle : ""
            enabled: playerMenu.session.activeCue >= 0
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.session.replay()
        }
        MenuSeparator {}
        MenuAction {
            text: "Previous video"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.previousVideo : ""
            enabled: playerMenu.session.queue.hasPrevious
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.session.queue.previous()
        }
        MenuAction {
            text: "Next video"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.nextVideo : ""
            enabled: playerMenu.session.queue.hasNext
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.session.queue.next()
        }
        Action {
            text: "Queue…"
            onTriggered: playerMenu.playerOptionsRequested(PlayerOptions.Queue)
        }
        Action {
            text: "Start over"
            enabled: playerMenu.session.hasMedia
            onTriggered: playerMenu.session.startOver()
        }
        MenuAction {
            text: "Jump to time…"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.jumpToTime : ""
            enabled: playerMenu.session.seekable
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.jumpToTimeRequested()
        }
        Action {
            text: "Undo last seek"
            enabled: playerMenu.session.seekable
            onTriggered: playerMenu.session.undoSeek()
        }
        Menu {
            id: chapterMenu
            title: "Chapters"
            enabled: playerMenu.session.chapters.length > 0
            Action {
                text: "Previous chapter"
                onTriggered: playerMenu.session.chapterStep(-1)
            }
            Action {
                text: "Next chapter"
                onTriggered: playerMenu.session.chapterStep(1)
            }
            MenuSeparator {}
            Instantiator {
                model: playerMenu.session.chapters
                delegate: MenuItem {
                    required property int index
                    required property var modelData
                    text: Theme.clock(modelData.time) + "  " + (modelData.title || "Chapter " + (index + 1))
                    onTriggered: playerMenu.session.jumpToChapter(index)
                }
                onObjectAdded: (index, object) => chapterMenu.insertItem(index + 3, object)
                onObjectRemoved: (index, object) => chapterMenu.removeItem(object)
            }
        }
        Action {
            text: "Previous frame"
            enabled: playerMenu.session.seekable
            onTriggered: playerMenu.session.frameStep(false)
        }
        Action {
            text: "Next frame"
            enabled: playerMenu.session.seekable
            onTriggered: playerMenu.session.frameStep(true)
        }
        Action {
            text: playerMenu.session.loopLabel
            enabled: playerMenu.session.seekable
            onTriggered: playerMenu.session.cycleLoop()
        }
        Action {
            text: "Slower by 0.05×"
            onTriggered: playerMenu.session.rate(playerMenu.session.playbackSpeed - 0.05)
        }
        Action {
            text: "Faster by 0.05×"
            onTriggered: playerMenu.session.rate(playerMenu.session.playbackSpeed + 0.05)
        }
        Action {
            text: "Reset playback speed"
            onTriggered: playerMenu.session.rate(1)
        }
        MenuSeparator {}
        Action {
            text: "Seek forward 5 seconds"
            enabled: playerMenu.session.hasMedia
            onTriggered: playerMenu.session.seek(playerMenu.session.position + 5)
        }
        Action {
            text: "Seek back 5 seconds"
            enabled: playerMenu.session.hasMedia
            onTriggered: playerMenu.session.seek(playerMenu.session.position - 5)
        }
        MenuSeparator {}
        Action {
            text: playerMenu.session.volumeLevel === 0 ? "Unmute" : "Mute"
            onTriggered: playerMenu.session.toggleMute()
        }
        MenuAction {
            text: "Volume up"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.volumeUp : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.session.volume(playerMenu.session.volumeLevel + 5)
        }
        MenuAction {
            text: "Volume down"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.volumeDown : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.session.volume(playerMenu.session.volumeLevel - 5)
        }
    }
    Menu {
        title: "Audio"
        Action {
            text: "Tracks and output…"
            onTriggered: playerMenu.playerOptionsRequested(PlayerOptions.Audio)
        }
        Action {
            text: "Audio earlier by 0.1 s"
            onTriggered: playerMenu.session.preferences.audioDelay -= 0.1
        }
        Action {
            text: "Audio later by 0.1 s"
            onTriggered: playerMenu.session.preferences.audioDelay += 0.1
        }
        Action {
            text: "Reset audio delay"
            onTriggered: playerMenu.session.preferences.audioDelay = 0
        }
    }
    Menu {
        title: "Video"
        Action {
            text: "Picture options…"
            onTriggered: playerMenu.playerOptionsRequested(PlayerOptions.Picture)
        }
        Action {
            text: "Save screenshot with subtitles…"
            enabled: playerMenu.session.hasMedia
            onTriggered: {
                playerMenu.screenshotRequested(true);
            }
        }
        Action {
            text: "Save clean screenshot…"
            enabled: playerMenu.session.hasMedia
            onTriggered: {
                playerMenu.screenshotRequested(false);
            }
        }
        Action {
            text: "Media information…"
            enabled: playerMenu.session.hasMedia
            onTriggered: playerMenu.playerOptionsRequested(PlayerOptions.Information)
        }
        Action {
            text: "Floating player"
            checkable: true
            checked: playerMenu.floatingMode
            onTriggered: playerMenu.floatingRequested()
        }
    }
    Menu {
        title: "Subtitles"
        Action {
            text: "Tracks, timing and encoding…"
            onTriggered: playerMenu.playerOptionsRequested(PlayerOptions.Subtitles)
        }
        MenuAction {
            text: "Load primary track…"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.primarySubtitles : ""
            enabled: playerMenu.session.hasMedia
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.primarySubtitlesRequested()
        }
        MenuAction {
            text: "Load secondary track…"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.secondarySubtitles : ""
            enabled: playerMenu.session.hasMedia
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.secondarySubtitlesRequested()
        }
        MenuSeparator {}
        Action {
            text: "Find subtitles…"
            enabled: playerMenu.session.hasMedia
            onTriggered: playerMenu.studyRequested(2)
        }
        Action {
            text: "Subtitle appearance…"
            onTriggered: playerMenu.settingsRequested(0)
        }
    }
    Menu {
        title: "Study"
        Action {
            text: "Explain current subtitle"
            enabled: playerMenu.session.subtitle.length > 0 || playerMenu.session.translation.length > 0
            onTriggered: playerMenu.session.explainCue(playerMenu.session.subtitle.length === 0)
        }
        Action {
            text: "Explain selection"
            enabled: playerMenu.session.selection.length > 0 || playerMenu.session.subtitle.length > 0
            onTriggered: {
                playerMenu.explanationRequested();
                playerMenu.session.explainSelection();
            }
        }
        MenuAction {
            text: "Dictionary"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.dictionary : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.dictionaryRequested()
        }
        MenuSeparator {}
        Action {
            text: "Flashcards"
            onTriggered: playerMenu.studyRequested(1)
        }
    }
    Menu {
        title: "View"
        MenuAction {
            text: playerMenu.transcriptOpen ? "Hide sidebar" : "Show sidebar"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.toggleSidebar : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.sidebarRequested()
        }
        MenuAction {
            text: "Recently played"
            shortcut: playerMenu.commandShortcutsEnabled ? playerMenu.keymap.bindings.recentVideos : ""
            onTriggered: if (playerMenu.commandShortcutsEnabled)
                playerMenu.recentVideosRequested()
        }
        Action {
            text: "Instant fullscreen"
            onTriggered: playerMenu.fullscreenController.toggleInstant()
        }
        MenuSeparator {}
        Action {
            text: "Playback preferences…"
            onTriggered: {
                playerMenu.settingsRequested(1);
            }
        }
        MenuItem {
            text: "Settings…"
            visible: Qt.platform.os !== "osx"
            onTriggered: playerMenu.settingsRequested(0)
        }
    }
    Menu {
        title: "Help"
        Action {
            text: "Getting started…"
            onTriggered: playerMenu.gettingStartedRequested()
        }
        Action {
            text: "Check for updates…"
            onTriggered: playerMenu.supportRequested(true)
        }
        Action {
            text: "Report a problem…"
            onTriggered: playerMenu.supportRequested(false)
        }
    }

}
