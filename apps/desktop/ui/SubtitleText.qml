pragma ComponentBehavior: Bound
import QtQuick
import Neden

Item {
    id: line
    required property SubtitleAppearance appearance
    property alias text: textEditor.text
    property int pixelSize: 28
    property bool selectable: true
    property bool explainEnabled: false
    property alias editor: textEditor
    signal selectionChanged(string selection)
    signal selectionCommitted
    signal phraseRequested(string phrase)
    readonly property real paintedWidth: outlineText.paintedWidth
    implicitHeight: textEditor.contentHeight
    visible: textEditor.length > 0

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
        color: line.appearance.textColor
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
        onSelectedTextChanged: {
            if (selectedText.length)
                line.selectionChanged(selectedText);
        }
        PointHandler {
            id: selectionPointer
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (!active && textEditor.selectedText.length)
                    Qt.callLater(line.selectionCommitted);
            }
        }
        Keys.onReleased: event => {
            if (event.key === Qt.Key_Shift && textEditor.selectedText.length)
                line.selectionCommitted();
        }
    }
    Item {
        id: phraseHoverRegion
        readonly property real textLeft: Math.max(0, (line.width - line.paintedWidth) / 2)
        readonly property real textRight: Math.min(line.width, (line.width + line.paintedWidth) / 2)
        x: Math.max(0, Math.min(textLeft - Theme.space2, explainLine.x))
        y: Math.min(0, explainLine.y)
        width: Math.min(line.width, Math.max(textRight + Theme.space2, explainLine.x + explainLine.width)) - x
        height: Math.max(line.height, explainLine.y + explainLine.height) - y
        enabled: line.explainEnabled
        HoverHandler {
            id: phraseHover
        }
    }
    ActionButton {
        id: explainLine
        objectName: line.objectName + "ExplainButton"
        readonly property bool fitsBesideText: phraseHoverRegion.textRight + Theme.space2 + width <= line.width
        text: "Explain line"
        implicitHeight: 30
        focusPolicy: Qt.TabFocus
        font.pixelSize: Theme.caption
        x: fitsBesideText ? phraseHoverRegion.textRight + Theme.space2 : line.width - width
        y: fitsBesideText ? (line.height - height) / 2 : -height - Theme.space2
        visible: line.explainEnabled && !selectionPointer.active && (phraseHover.hovered || hovered || activeFocus)
        Accessible.name: "Explain the full subtitle"
        onClicked: line.phraseRequested(line.text)
    }
}
