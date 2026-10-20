import QtQuick
import Neden

Item {
    id: shortcuts
    required property LearningSession session
    property var bindings: ({instantFullscreen: "F", togglePlayback: "Space", toggleMute: "M", seekForward: "Right", seekBackward: "Left", explainSubtitle: "E"})
    signal explainSubtitleRequested
    signal instantFullscreenRequested
    property bool available: true
    property bool textEntryActive: false
    property bool controlKeysActive: false
    property bool subtitleSelectionActive: false
    readonly property bool fullscreenKeyEnabled: available && session.hasMedia && !textEntryActive
    readonly property bool playbackKeysEnabled: available && session.hasMedia && !textEntryActive && !controlKeysActive
    objectName: "playerShortcuts"
    Shortcut {
        sequence: shortcuts.bindings.explainSubtitle
        enabled: shortcuts.available && !shortcuts.textEntryActive && (shortcuts.session.subtitle.length > 0 || shortcuts.session.translation.length > 0)
        onActivated: shortcuts.explainSubtitleRequested()
    }
    Shortcut {
        sequence: shortcuts.bindings.instantFullscreen
        enabled: shortcuts.fullscreenKeyEnabled
        onActivated: shortcuts.instantFullscreenRequested()
    }
    Shortcut {
        sequence: shortcuts.bindings.togglePlayback
        enabled: shortcuts.playbackKeysEnabled
        onActivated: shortcuts.session.toggle()
    }
    Shortcut {
        sequence: shortcuts.bindings.toggleMute
        enabled: shortcuts.playbackKeysEnabled
        onActivated: shortcuts.session.toggleMute()
    }
    Shortcut {
        sequence: shortcuts.bindings.seekForward
        enabled: shortcuts.playbackKeysEnabled && !shortcuts.subtitleSelectionActive
        onActivated: shortcuts.session.seek(shortcuts.session.position + 5)
    }
    Shortcut {
        sequence: shortcuts.bindings.seekBackward
        enabled: shortcuts.playbackKeysEnabled && !shortcuts.subtitleSelectionActive
        onActivated: shortcuts.session.seek(shortcuts.session.position - 5)
    }
}
