pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#101416"
    readonly property color surface: "#191f22"
    readonly property color elevated: "#222b2d"
    readonly property color border: "#303b3d"
    readonly property color ink: "#edf2ef"
    readonly property color muted: "#9caeaa"
    readonly property color accent: "#b6e5c9"
    function clock(seconds) {
        return Math.floor(seconds / 60).toString().padStart(2, "0") + ":" + Math.floor(seconds % 60).toString().padStart(2, "0");
    }
}
