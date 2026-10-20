pragma Singleton
import QtQuick

QtObject {
    // Shared color, type, spacing, and shape tokens for every application surface.
    readonly property color background: "#10151b"
    readonly property color surface: "#171d25"
    readonly property color elevated: "#202731"
    readonly property color hover: "#2a3441"
    readonly property color border: "#303844"
    readonly property color ink: "#e6eaf2"
    readonly property color muted: "#9aa7b5"
    readonly property color accent: "#429df5"
    readonly property color selection: "#294665"
    readonly property color wordHover: "#30429df5"
    readonly property color selected: "#1e3041"
    readonly property color error: "#f0b4ab"
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 24
    readonly property int space6: 32
    readonly property int caption: 12
    readonly property int body: 14
    readonly property int quotation: 15
    readonly property int title: 20
    readonly property int heading: 24
    readonly property int controlHeight: 38
    readonly property int controlRadius: 4
    readonly property int panelRadius: 6

    function clock(seconds) {
        const total = Math.max(0, Math.floor(seconds));
        const minutes = Math.floor(total / 60);
        const tail = (total % 60).toString().padStart(2, "0");
        return total >= 3600 ? Math.floor(total / 3600) + ":" + (minutes % 60).toString().padStart(2, "0") + ":" + tail : minutes.toString().padStart(2, "0") + ":" + tail;
    }
}
