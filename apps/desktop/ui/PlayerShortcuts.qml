import QtQuick
import Neden

Item {
    id: shortcuts
    required property LearningSession session
    required property ShortcutMap keymap
    readonly property var bindings: keymap.bindings
    // Read-only text otherwise consumes arrows to move a caret that is not visible.
    function handleSeekKey(event, activate) {
        if (!playbackKeysEnabled || subtitleDragActive)
            return false;
        const forward = keymap.matches("seekForward", event.key, event.modifiers);
        const backward = keymap.matches("seekBackward", event.key, event.modifiers);
        if (!forward && !backward)
            return false;
        if (activate) {
            if (forward)
                seekForward.activated();
            else
                seekBackward.activated();
        }
        return true;
    }
    signal explainSubtitleRequested
    signal instantFullscreenRequested
    property bool available: true
    property bool textEntryActive: false
    property bool controlKeysActive: false
    property bool subtitleDragActive: false
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
        id: seekForward
        objectName: "seekForwardShortcut"
        sequence: shortcuts.bindings.seekForward
        enabled: shortcuts.playbackKeysEnabled && !shortcuts.subtitleDragActive
        onActivated: shortcuts.session.seek(shortcuts.session.position + 5)
    }
    Shortcut {
        id: seekBackward
        objectName: "seekBackwardShortcut"
        sequence: shortcuts.bindings.seekBackward
        enabled: shortcuts.playbackKeysEnabled && !shortcuts.subtitleDragActive
        onActivated: shortcuts.session.seek(shortcuts.session.position - 5)
    }
}
