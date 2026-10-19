pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Neden

Rectangle {
    id: stage
    required property LearningSession session
    required property MpvPlayback playback
    required property SubtitleAppearance appearance
    signal explainSelection
    property bool dictionaryOpen: false
    readonly property bool editing: subtitle.editor.activeFocus || translation.editor.activeFocus
    signal openDictionary
    signal closeSelection
    color: "#080b0c"
    radius: 14
    clip: true
    function selectAll() {
        subtitle.editor.forceActiveFocus();
        subtitle.editor.selectAll();
    }
    function deselect() {
        subtitle.editor.deselect();
        translation.editor.deselect();
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
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        anchors.bottomMargin: stage.appearance.bottomMargin
        height: subtitleColumn.implicitHeight
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width, Math.max(subtitle.paintedWidth, translation.paintedWidth) + 24)
            height: parent.height + 16
            radius: Theme.controlRadius
            color: "#aa101416"
            visible: stage.appearance.background && (subtitle.visible || translation.visible)
        }
        ColumnLayout {
            id: subtitleColumn
            width: parent.width
            spacing: Theme.space3
            SubtitleText {
                id: subtitle
                objectName: "primarySubtitleText"
                Layout.fillWidth: true
                text: stage.session.subtitle
                appearance: stage.appearance
                pixelSize: stage.appearance.primarySize
                onSelectionChanged: selection => stage.session.select(selection)
                onSelectionCommitted: stage.explainSelection()
            }
            SubtitleText {
                id: translation
                objectName: "secondarySubtitleText"
                Layout.fillWidth: true
                text: stage.session.translation
                appearance: stage.appearance
                pixelSize: stage.appearance.secondarySize
                onSelectionChanged: selection => stage.session.select(selection)
                onSelectionCommitted: stage.explainSelection()
            }
        }
    }
    Connections {
        target: stage.session
        function onSelectionChanged() {
            if (!stage.session.selection.length)
                subtitle.editor.deselect();
            if (!stage.session.selection.length)
                translation.editor.deselect();
        }
    }
}
