import QtQuick

Rectangle {
    id: sidebar
    property bool expanded: false
    property bool reduceMotion: false
    property bool ready: false
    property real revealProgress: 0
    // The video resizes once on opening and once after closing, never per frame.
    readonly property real reservedWidth: expanded || revealProgress > 0 ? width : 0
    x: parent.width - width * revealProgress
    height: parent.height
    visible: expanded || revealProgress > 0
    enabled: expanded
    function updateReveal() {
        if (!ready) {
            revealProgress = expanded ? 1 : 0;
            return;
        }
        slide.stop();
        if (reduceMotion)
            revealProgress = expanded ? 1 : 0;
        else {
            slide.to = expanded ? 1 : 0;
            slide.start();
        }
    }
    onExpandedChanged: updateReveal()
    onReduceMotionChanged: updateReveal()
    Component.onCompleted: {
        revealProgress = expanded ? 1 : 0;
        ready = true;
    }
    NumberAnimation {
        id: slide
        target: sidebar
        property: "revealProgress"
        duration: 180
        easing.type: Easing.OutCubic
    }
}
