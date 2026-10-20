pragma ComponentBehavior: Bound
import QtQuick
import Neden

Item {
    id: line
    required property SubtitleAppearance appearance
    required property WordLookup wordLookup
    property alias text: textEditor.text
    property int pixelSize: 28
    property bool selectable: true
    property color textColor: appearance.textColor
    property alias editor: textEditor
    signal selectionChanged(string selection)
    signal selectionCommitted
    signal wordRequested(int offset)
    property int wordOffset: -1
    property string wordText: ""
    readonly property bool selecting: selectionPointer.active
    readonly property real paintedWidth: outlineText.paintedWidth
    implicitHeight: textEditor.contentHeight
    visible: textEditor.length > 0

    // Hover is presentation only: it never changes selection, focus, or requests a definition.
    readonly property var hoverRects: {
        const layoutWidth = textEditor.width;
        const layoutFont = textEditor.font;
        if (!wordHover.hovered || !selectable || selecting || textEditor.selectedText.length)
            return [];
        const point = wordHover.point.position;
        const range = wordLookup.wordRangeAt(textEditor.text, textEditor.positionAt(point.x, point.y));
        if (range.start === undefined)
            return [];
        const rects = [];
        for (let i = range.start; i < range.end; ++i) {
            const first = textEditor.positionToRectangle(i);
            const last = textEditor.positionToRectangle(i + 1);
            const sameRow = Math.abs(first.y - last.y) < 1;
            const left = sameRow ? Math.min(first.x, last.x) : first.x;
            const right = sameRow ? Math.max(first.x, last.x) : first.x + fontMetrics.advanceWidth(textEditor.text.substring(i, i + 1));
            if (right - left < 0.1)
                continue;
            const previous = rects.length ? rects[rects.length - 1] : null;
            if (previous && Math.abs(previous.y - first.y) < 1 && left <= previous.x + previous.width + 1 && right >= previous.x - 1) {
                const edge = Math.max(previous.x + previous.width, right);
                previous.x = Math.min(previous.x, left);
                previous.width = edge - previous.x;
            } else
                rects.push({x: left, y: first.y, width: right - left, height: first.height});
        }
        // positionAt clamps to the nearest caret even in the empty margins.
        return rects.some(rect => point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height) ? rects : [];
    }
    FontMetrics { id: fontMetrics; font: textEditor.font }
    Repeater {
        model: line.hoverRects
        delegate: Rectangle {
            required property var modelData
            objectName: "subtitleWordHover"
            x: modelData.x - 2
            y: modelData.y
            width: modelData.width + 4
            height: modelData.height
            radius: 3
            color: Theme.wordHover
        }
    }

    // Outline and selection use the same text layout; no full-video effect texture.
    Text {
        id: outlineText
        anchors.fill: parent
        text: textEditor.text
        textFormat: Text.PlainText
        font: textEditor.font
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        renderType: Text.QtRendering
        color: line.textColor
        style: line.appearance.outline ? Text.Outline : Text.Normal
        styleColor: "#111111"
    }
    TextEdit {
        id: textEditor
        anchors.fill: parent
        textFormat: TextEdit.PlainText
        readOnly: true
        selectByMouse: line.selectable
        persistentSelection: true
        wrapMode: TextEdit.Wrap
        horizontalAlignment: TextEdit.AlignHCenter
        font.family: "Inter"
        font.pixelSize: line.pixelSize
        font.weight: line.appearance.bold ? Font.DemiBold : Font.Normal
        renderType: TextEdit.QtRendering
        color: "transparent"
        selectionColor: Theme.selection
        selectedTextColor: "white"
        HoverHandler {
            id: wordHover
            blocking: false
            cursorShape: line.selectable ? Qt.IBeamCursor : Qt.ArrowCursor
        }
        onTextChanged: wordDelay.stop()
        onSelectedTextChanged: {
            if (selectedText.length) {
                wordDelay.stop();
                line.selectionChanged(selectedText);
            }
        }
        PointHandler {
            id: selectionPointer
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active) {
                    wordDelay.stop();
                    line.wordText = textEditor.text;
                    line.wordOffset = textEditor.positionAt(point.position.x, point.position.y);
                } else if (line.selectable && !textEditor.selectedText.length)
                    Qt.callLater(() => {
                        if (line.wordText === textEditor.text && !textEditor.selectedText.length)
                            wordDelay.restart();
                    });
                if (!active && textEditor.selectedText.length)
                    Qt.callLater(line.selectionCommitted);
            }
        }
        Keys.onReleased: event => {
            if (event.key === Qt.Key_Shift && textEditor.selectedText.length)
                line.selectionCommitted();
        }
    }
    Timer {
        id: wordDelay
        interval: Math.max(350, Application.styleHints.mouseDoubleClickInterval + 25)
        onTriggered: {
            if (!textEditor.selectedText.length)
                line.wordRequested(line.wordOffset);
        }
    }
}
