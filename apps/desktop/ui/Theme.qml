pragma Singleton
import QtQuick

QtObject {
    // Shared color, type, spacing, and shape tokens for every application surface.
    readonly property color background: "#101416"
    readonly property color surface: "#191f22"
    readonly property color elevated: "#222b2d"
    readonly property color hover: "#2b3739"
    readonly property color border: "#303b3d"
    readonly property color ink: "#edf2ef"
    readonly property color muted: "#9caeaa"
    readonly property color accent: "#b6e5c9"
    readonly property color selection: "#365c4d"
    readonly property color error: "#f0b4ab"
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 24
    readonly property int space6: 32
    readonly property int caption: 12
    readonly property int body: 14
    readonly property int title: 20
    readonly property int heading: 24
    readonly property int controlHeight: 38
    readonly property int controlRadius: 8
    readonly property int panelRadius: 12

    function clock(seconds) {
        return Math.floor(seconds / 60).toString().padStart(2, "0") + ":" + Math.floor(seconds % 60).toString().padStart(2, "0");
    }
}
