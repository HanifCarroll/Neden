import QtQuick

// Small original vector marks for player controls.
Canvas {
    id: glyph
    property string name: "play"
    property color strokeColor: Theme.ink
    implicitWidth: 16
    implicitHeight: 16
    onNameChanged: requestPaint()
    onStrokeColorChanged: requestPaint()
    onPaint: {
        const painter = getContext("2d");
        painter.reset();
        painter.strokeStyle = strokeColor;
        painter.lineWidth = 1.5;
        painter.lineCap = "round";
        painter.lineJoin = "round";
        painter.beginPath();
        if (name === "play") {
            painter.moveTo(5, 3);
            painter.lineTo(12, 8);
            painter.lineTo(5, 13);
            painter.closePath();
        } else if (name === "pause") {
            painter.moveTo(5, 3);
            painter.lineTo(5, 13);
            painter.moveTo(11, 3);
            painter.lineTo(11, 13);
        } else if (name === "chevron") {
            painter.moveTo(4, 6);
            painter.lineTo(8, 10);
            painter.lineTo(12, 6);
        }
        painter.stroke();
    }
}
