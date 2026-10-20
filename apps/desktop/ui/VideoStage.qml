pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Neden

Rectangle {
    id: stage
    required property LearningSession session
    required property MpvPlayback playback
    required property SubtitleAppearance appearance
    property bool windowDragEnabled: false
    signal fullscreenRequested
    signal moveWindowRequested
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
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        enabled: stage.session.hasMedia
        onClicked: stage.forceActiveFocus()
        onDoubleClicked: stage.fullscreenRequested()
    }
    Item {
        width: parent.width
        height: 28
        visible: stage.windowDragEnabled
        DragHandler {
            target: null
            onActiveChanged: {
                if (active) stage.moveWindowRequested();
            }
        }
    }
    HoverHandler {
        id: videoHover
        blocking: false
    }
    Item {
        id: subtitleOverlay
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        readonly property real videoAspect: stage.playback.sourceOutputSize.height > 0 ? stage.playback.sourceOutputSize.width / stage.playback.sourceOutputSize.height : 16 / 9
        readonly property real pictureHeight: Math.min(stage.height, stage.width / videoAspect)
        anchors.bottomMargin: (stage.height - pictureHeight) / 2 + stage.appearance.bottomMargin + pictureHeight * stage.appearance.bottomPercent / 100
        height: subtitleColumn.implicitHeight
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width, Math.max(subtitle.paintedWidth, translation.paintedWidth) + 24)
            height: parent.height + 16
            radius: Theme.controlRadius
            color: "#aa101416"
            visible: stage.appearance.background && (subtitle.visible || translation.visible)
        }
        GridLayout {
            id: subtitleColumn
            columns: 1
            width: parent.width
            rowSpacing: stage.appearance.gap
            SubtitleText {
                id: subtitle
                objectName: "primarySubtitleText"
                Layout.fillWidth: true
                text: stage.session.subtitle
                appearance: stage.appearance
                pixelSize: stage.appearance.primarySize
                selectable: stage.appearance.selectable
                Layout.row: stage.appearance.reversed ? 1 : 0
                onWordRequested: offset => stage.session.lookupWordAt(text, offset, false)
                onSelectionChanged: selection => stage.session.select(selection)
                onSelectionCommitted: stage.session.commitSelection(false)
            }
            SubtitleText {
                id: translation
                objectName: "secondarySubtitleText"
                Layout.fillWidth: true
                text: stage.session.translation
                appearance: stage.appearance
                pixelSize: stage.appearance.secondarySize
                selectable: stage.appearance.selectable
                textColor: stage.appearance.secondaryColor
                Layout.row: stage.appearance.reversed ? 0 : 1
                onWordRequested: offset => stage.session.lookupWordAt(text, offset, true)
                onSelectionChanged: selection => stage.session.select(selection, true)
                onSelectionCommitted: stage.session.commitSelection(true)
            }
        }
    }
    ActionButton {
        id: explainLine
        objectName: "primarySubtitleTextExplainButton"
        readonly property real textRight: subtitleOverlay.x + subtitle.x + Math.min(subtitle.width, (subtitle.width + subtitle.paintedWidth) / 2)
        readonly property bool fitsBesideText: textRight + Theme.space2 + width <= stage.width - Theme.space4
        readonly property real preferredY: fitsBesideText ? subtitleOverlay.y + subtitle.y + (subtitle.height - height) / 2 : subtitleOverlay.y - height - Theme.space2
        // Keep one hover target across both tracks and the detached action.
        readonly property bool captionHovered: {
            if (!videoHover.hovered)
                return false;

            const position = videoHover.point.position;
            const left = Math.min(subtitleOverlay.x, x) - Theme.space3;
            const right = Math.max(subtitleOverlay.x + subtitleOverlay.width, x + width) + Theme.space3;
            const top = Math.min(subtitleOverlay.y, y) - Theme.space3;
            const bottom = Math.max(subtitleOverlay.y + subtitleOverlay.height, y + height) + Theme.space3;
            return position.x >= left && position.x <= right && position.y >= top && position.y <= bottom;
        }
        property bool hoverRetained: false
        text: "Explain line"
        implicitHeight: 30
        focusPolicy: Qt.TabFocus
        font.pixelSize: Theme.caption
        x: Math.max(Theme.space2, Math.min(fitsBesideText ? textRight + Theme.space2 : textRight - width, stage.width - width - Theme.space2))
        y: Math.max(Theme.space2, Math.min(preferredY, stage.height - height - Theme.space2))
        visible: stage.appearance.selectable && subtitle.visible && !subtitle.selecting && (captionHovered || hoverRetained || hovered || activeFocus)
        Accessible.name: "Explain the full primary subtitle"
        onCaptionHoveredChanged: {
            if (captionHovered) {
                hideExplanation.stop();
                hoverRetained = true;
            } else
                hideExplanation.restart();
        }
        onClicked: {
            stage.session.explainCue();
        }
    }
    Timer {
        id: hideExplanation
        interval: 180
        onTriggered: explainLine.hoverRetained = false
    }
    Rectangle {
        id: wordCard
        objectName: "wordMeaningCard"
        visible: stage.session.study.words.state !== "idle"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.max(16, Math.min(parent.height - height - 16,
            subtitleOverlay.height + subtitleOverlay.anchors.bottomMargin + 12))
        width: Math.min(400, parent.width - 32)
        height: Math.min(wordContents.implicitHeight + 32, parent.height - 32)
        radius: Theme.panelRadius
        color: Theme.surface
        border.color: Theme.border
        // Keep clicks inside the floating card from reaching the video gestures.
        MouseArea { anchors.fill: parent }
        Flickable {
            anchors.fill: parent
            anchors.margins: 16
            contentHeight: wordContents.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: wordContents
                width: parent.width
                spacing: Theme.space2
                RowLayout {
                    Label {
                        text: stage.session.study.words.word
                        textFormat: Text.PlainText
                        color: Theme.ink
                        font.pixelSize: Theme.title
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    IconButton {
                        name: "x"
                        description: "Close word meaning"
                        objectName: "closeWordCardButton"
                        onClicked: stage.session.closeWordCard()
                    }
                }
                Label {
                    visible: text.length > 0 && text !== stage.session.study.words.word
                    text: stage.session.study.words.lemma
                    textFormat: Text.PlainText
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Repeater {
                    model: stage.session.study.words.meanings
                    delegate: Label {
                        required property string modelData
                        objectName: "wordContextMeaning"
                        text: modelData
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: Theme.ink
                        font.weight: Font.DemiBold
                    }
                }
                Label {
                    text: stage.session.study.words.contextNote
                    visible: text.length > 0
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    Layout.fillWidth: true
                }
                Label {
                    visible: stage.session.study.words.otherMeanings.length > 0
                    text: "Other meanings"
                    color: Theme.ink
                    font.weight: Font.DemiBold
                    Layout.topMargin: Theme.space2
                }
                Repeater {
                    model: stage.session.study.words.otherMeanings
                    delegate: Label {
                        required property string modelData
                        text: modelData
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: Theme.muted
                    }
                }
                Label {
                    visible: stage.session.study.words.state !== "complete"
                    text: stage.session.study.words.state === "loading"
                        ? (stage.session.study.words.contextual ? "Finding the meaning in this scene…" : "Looking up…")
                        : stage.session.study.words.message
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: "Try again"
                    visible: stage.session.study.words.contextual && stage.session.study.words.state === "failed"
                    onClicked: stage.session.retryWordLookup()
                }
                ActionButton {
                    text: "Try word translation"
                    visible: stage.session.study.settings.wordProvider === "microsoft" && stage.session.study.words.state === "failed"
                    onClicked: stage.session.translateWord()
                }
                ActionButton {
                    text: "Full explanation"
                    objectName: "wordFullExplanationButton"
                    flat: true
                    onClicked: {
                        stage.session.explainWord();
                    }
                }
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
