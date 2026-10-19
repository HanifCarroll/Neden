pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Rectangle {
    id: stage
    required property LearningSession session
    required property MpvPlayback playback
    property bool dictionaryOpen: false
    readonly property bool editing: subtitle.activeFocus
    signal openDictionary
    signal closeSelection
    color: "#080b0c"
    radius: 14
    clip: true
    function selectAll() {
        subtitle.forceActiveFocus();
        subtitle.selectAll();
    }
    function deselect() {
        subtitle.deselect();
    }
    VideoSurface {
        objectName: "videoSurface"
        anchors.fill: parent
        playback: stage.playback
        mirrorVertically: true
    }
    // Four small corner masks keep the video surface rounded without a full-size texture layer.
    Repeater {
        model: 4
        delegate: Canvas {
            required property int index
            width: 14
            height: 14
            x: index === 0 || index === 3 ? 0 : stage.width - width
            y: index < 2 ? 0 : stage.height - height
            rotation: index * 90
            onPaint: {
                const painter = getContext("2d");
                painter.fillStyle = Theme.background;
                painter.beginPath();
                painter.moveTo(0, 0);
                painter.lineTo(14, 0);
                painter.arc(14, 14, 14, -Math.PI / 2, -Math.PI, true);
                painter.closePath();
                painter.fill();
            }
        }
    }
    SelectionPanel {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 20
        width: 390
        session: stage.session
        visible: stage.session.selection.length > 0 && !stage.dictionaryOpen
        onOpenDictionary: stage.openDictionary()
        onDismissed: stage.closeSelection()
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        height: subtitleColumn.implicitHeight + 28
        radius: 12
        color: "#e6101416"
        ColumnLayout {
            id: subtitleColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 18
            spacing: 7
            TextEdit {
                id: subtitle
                Layout.fillWidth: true
                text: stage.session.subtitle || "Open a video to begin"
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                horizontalAlignment: TextEdit.AlignHCenter
                font.pixelSize: 24
                color: Theme.ink
                selectionColor: "#467b68"
                selectedTextColor: "white"
                onSelectedTextChanged: {
                    if (selectedText.length)
                        stage.session.select(selectedText);
                }
            }
            Label {
                Layout.fillWidth: true
                text: stage.session.translation
                textFormat: Text.PlainText
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
    Connections {
        target: stage.session
        function onSelectionChanged() {
            if (!stage.session.selection.length)
                subtitle.deselect();
        }
    }
}
