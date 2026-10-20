import QtQuick
import Neden

Item {
    id: shortcuts
    required property LearningSession session
    signal instantFullscreenRequested
    property bool available: true
    property bool textEntryActive: false
    property bool controlKeysActive: false
    property bool subtitleSelectionActive: false
    readonly property bool playbackKeysEnabled: available && session.hasMedia && !textEntryActive && !controlKeysActive
    objectName: "playerShortcuts"
    Shortcut {
        sequence: "F"
        enabled: shortcuts.playbackKeysEnabled
        onActivated: shortcuts.instantFullscreenRequested()
    }
    Shortcut {
        sequence: "Space"
        enabled: shortcuts.playbackKeysEnabled
        onActivated: shortcuts.session.toggle()
    }
    Shortcut {
        sequence: "M"
        enabled: shortcuts.playbackKeysEnabled
        onActivated: shortcuts.session.toggleMute()
    }
    Shortcut {
        sequence: "Right"
        enabled: shortcuts.playbackKeysEnabled && !shortcuts.subtitleSelectionActive
        onActivated: shortcuts.session.seek(shortcuts.session.position + 5)
    }
    Shortcut {
        sequence: "Left"
        enabled: shortcuts.playbackKeysEnabled && !shortcuts.subtitleSelectionActive
        onActivated: shortcuts.session.seek(shortcuts.session.position - 5)
    }
}
