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
    readonly property bool hovered: wordHover.hovered
    readonly property bool wordsHovered: hovered && wordRectanglesAt(wordHover.point.position, textEditor.width, textEditor.font).length > 0
    readonly property real paintedWidth: outlineText.paintedWidth
    implicitHeight: textEditor.contentHeight
    visible: textEditor.length > 0

    // Hover is presentation only: it never changes selection, focus, or requests a definition.
    readonly property var hoverRects: {
        if (!hovered || !selectable || selecting || textEditor.selectedText.length)
            return [];
        return wordRectanglesAt(wordHover.point.position, textEditor.width, textEditor.font);
    }
    function containsPoint(rectangle, point) {
        return point.x >= rectangle.x && point.x <= rectangle.x + rectangle.width && point.y >= rectangle.y && point.y <= rectangle.y + rectangle.height;
    }
    function wordRectanglesAt(pointerPosition, layoutWidth, layoutFont) {
        if (layoutWidth <= 0 || layoutFont.pixelSize <= 0)
            return [];
        const wordRange = wordLookup.wordRangeAt(textEditor.text, textEditor.positionAt(pointerPosition.x, pointerPosition.y));
        if (wordRange.start === undefined)
            return [];
        const rectangles = [];
        for (let characterIndex = wordRange.start; characterIndex < wordRange.end; ++characterIndex) {
            const characterStart = textEditor.positionToRectangle(characterIndex);
            const characterEnd = textEditor.positionToRectangle(characterIndex + 1);
            const sameLine = Math.abs(characterStart.y - characterEnd.y) < 1;
            const leftEdge = sameLine ? Math.min(characterStart.x, characterEnd.x) : characterStart.x;
            const rightEdge = sameLine ? Math.max(characterStart.x, characterEnd.x) : characterStart.x + fontMetrics.advanceWidth(textEditor.text.substring(characterIndex, characterIndex + 1));
            if (rightEdge - leftEdge < 0.1)
                continue;
            const previousRectangle = rectangles.length ? rectangles[rectangles.length - 1] : null;
            const touchesPrevious = previousRectangle && Math.abs(previousRectangle.y - characterStart.y) < 1 && leftEdge <= previousRectangle.x + previousRectangle.width + 1 && rightEdge >= previousRectangle.x - 1;
            if (touchesPrevious) {
                const combinedRightEdge = Math.max(previousRectangle.x + previousRectangle.width, rightEdge);
                previousRectangle.x = Math.min(previousRectangle.x, leftEdge);
                previousRectangle.width = combinedRightEdge - previousRectangle.x;
            } else {
                rectangles.push({
                    x: leftEdge,
                    y: characterStart.y,
                    width: rightEdge - leftEdge,
                    height: characterStart.height
                });
            }
        }
        // positionAt clamps to the nearest caret even in the empty margins.
        for (const rectangle of rectangles) {
            if (containsPoint(rectangle, pointerPosition))
                return rectangles;
        }
        return [];
    }
    FontMetrics {
        id: fontMetrics
        font: textEditor.font
    }
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
