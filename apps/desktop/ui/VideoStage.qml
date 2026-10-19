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
    readonly property bool editing: subtitle.editor.activeFocus || translation.editor.activeFocus
    color: "#080b0c"
    radius: 0
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
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        readonly property real videoAspect: stage.playback.sourceOutputSize.height > 0 ? stage.playback.sourceOutputSize.width / stage.playback.sourceOutputSize.height : 16 / 9
        readonly property real pictureHeight: Math.min(stage.height, stage.width / videoAspect)
        anchors.bottomMargin: (stage.height - pictureHeight) / 2 + stage.appearance.bottomMargin
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
