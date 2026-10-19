pragma ComponentBehavior: Bound
import QtQuick
import Neden

Item {
    id: line
    required property SubtitleAppearance appearance
    property alias text: textEditor.text
    property int pixelSize: 28
    property bool selectable: true
    property alias editor: textEditor
    signal selectionChanged(string selection)
    signal selectionCommitted
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
}
