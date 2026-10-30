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
    property var seekKeyHandler: null
    property bool capturing: false
    readonly property bool wordCardOpen: wordCard.visible
    property real bottomOverlayHeight: 0
    readonly property bool selecting: subtitle.selecting || translation.selecting
    signal screenshotFinished(bool saved)
    function saveScreenshot(path) {
        if (!path.length)
            return;
        capturing = true;
        Qt.callLater(() => {
            const started = stage.grabToImage(result => {
                const saved = result.saveToFile(path);
                stage.capturing = false;
                stage.screenshotFinished(saved);
            });
            if (!started) {
                stage.capturing = false;
                stage.screenshotFinished(false);
            }
        });
    }
    property bool windowDragEnabled: false
    signal videoClicked
    signal fullscreenRequested
    signal moveWindowRequested
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
        objectName: "videoClickArea"
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        enabled: stage.session.hasMedia
        onClicked: {
            stage.forceActiveFocus();
            singleClick.restart();
        }
        onDoubleClicked: {
            singleClick.stop();
            stage.fullscreenRequested();
        }
        Timer {
            id: singleClick
            interval: Application.styleHints.mouseDoubleClickInterval
            onTriggered: stage.videoClicked()
        }
    }
    Item {
        width: parent.width
        height: 28
        visible: stage.windowDragEnabled
        DragHandler {
            target: null
            onActiveChanged: {
                if (active)
                    stage.moveWindowRequested();
            }
        }
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
        readonly property real preferredBottomMargin: (stage.height - pictureHeight) / 2 + stage.appearance.bottomMargin + pictureHeight * stage.appearance.bottomPercent / 100
        readonly property real controlsClearance: !stage.capturing && stage.bottomOverlayHeight > 0 ? stage.bottomOverlayHeight + Theme.space3 : 0
        anchors.bottomMargin: Math.max(preferredBottomMargin, controlsClearance)
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
                seekKeyHandler: stage.seekKeyHandler
                wordLookup: stage.session.study.words
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
                seekKeyHandler: stage.seekKeyHandler
                wordLookup: stage.session.study.words
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
    ColumnLayout {
        anchors.centerIn: parent
        visible: !stage.capturing && stage.session.playbackState === "finished"
        Label {
            text: "Playback finished"
            color: "white"
            font.pixelSize: Theme.title
        }
        RowLayout {
            ActionButton {
                text: "Replay"
                onClicked: stage.session.startOver()
            }
            ActionButton {
                text: "Next video"
                visible: stage.session.queue.hasNext
                onClicked: stage.session.queue.next()
            }
        }
    }
    Label {
        anchors.centerIn: parent
        visible: !stage.capturing && ["loading", "buffering", "seeking"].includes(stage.session.playbackState)
        text: stage.session.playbackState === "loading" ? "Opening video…" : stage.session.playbackState === "buffering" ? "Buffering…" : "Seeking…"
        color: "white"
        padding: Theme.space3
        background: Rectangle {
            color: "#bb10141a"
            radius: 8
        }
    }
    // Keep this hit target present while the action is hidden. It observes hover
    // without taking clicks or text-selection grabs away from the controls below.
    Item {
        z: 1
        x: explainLine.x
        y: explainLine.y
        width: explainLine.width
        height: explainLine.height
        visible: !stage.capturing && explainLine.hasCaptions
        HoverHandler {
            id: explainFootprint
            blocking: false
            cursorShape: Qt.PointingHandCursor
        }
    }
    ActionButton {
        id: explainLine
        objectName: "primarySubtitleTextExplainButton"
        z: 2 // A floating word card must not cover this action.
        readonly property bool hasCaptions: subtitle.visible || translation.visible
        readonly property SubtitleText caption: subtitle.visible ? subtitle : translation
        readonly property real textRight: subtitleOverlay.x + caption.x + Math.min(caption.width, (caption.width + caption.paintedWidth) / 2)
        readonly property bool fitsBesideText: textRight + Theme.space2 + width <= stage.width - Theme.space4
        readonly property real preferredX: fitsBesideText ? textRight + Theme.space2 : textRight - width
        readonly property real preferredY: fitsBesideText ? subtitleOverlay.y + caption.y + (caption.height - height) / 2 : subtitleOverlay.y - height - Theme.space2
        readonly property bool captionHovered: (subtitle.visible && subtitle.wordsHovered) || (translation.visible && translation.wordsHovered)
        readonly property bool actionInUse: hovered || down
        readonly property bool shouldShow: captionHovered || explainFootprint.hovered || hoverRetained || actionInUse
        property bool hoverRetained: false
        text: "Explain line"
        implicitHeight: 30
        focusPolicy: Qt.NoFocus
        font.pixelSize: Theme.caption
        x: Math.max(Theme.space2, Math.min(preferredX, stage.width - width - Theme.space2))
        y: Math.max(Theme.space2, Math.min(preferredY, stage.height - height - Theme.space2))
        visible: !stage.capturing && hasCaptions && shouldShow
        background: Rectangle {
            color: explainLine.down ? Theme.accent : explainLine.hovered ? Theme.hover : Theme.surface
            radius: Theme.controlRadius
            border.color: explainLine.activeFocus ? Theme.accent : Theme.border
        }
        Accessible.name: subtitle.visible ? "Explain the full primary subtitle" : "Explain the full secondary subtitle"
        onCaptionHoveredChanged: {
            if (captionHovered) {
                hideExplanation.stop();
                hoverRetained = true;
            } else
                hideExplanation.restart();
        }
        onClicked: {
            stage.session.explainCue(!subtitle.visible);
        }
    }
    // Briefly retain the action while crossing the small gap from text to button.
    // Empty caption margins never activate it, even while text is selected.
    Timer {
        id: hideExplanation
        interval: 180
        onTriggered: explainLine.hoverRetained = false
    }
    Rectangle {
        id: wordCard
        property bool detailsExpanded: false
        property string displayedWord: ""
        readonly property bool hasDetails: stage.session.study.words.formNote.length > 0
            || stage.session.study.words.contextNote.length > 0
            || stage.session.study.words.otherSenses.length > 0
            || stage.session.study.words.otherUses.length > 0
        Connections {
            target: stage.session.study.words
            function onChanged() {
                if (wordCard.displayedWord !== stage.session.study.words.word || stage.session.study.words.state !== "complete")
                    wordCard.detailsExpanded = false;
                wordCard.displayedWord = stage.session.study.words.word;
            }
        }
        objectName: "wordMeaningCard"
        visible: !stage.capturing && stage.session.study.words.state !== "idle"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Math.max(16, Math.min(parent.height - height - 16, subtitleOverlay.height + subtitleOverlay.anchors.bottomMargin + 12))
        width: Math.min(340, parent.width - 32)
        height: Math.min(wordContents.implicitHeight + 32, parent.height - 32)
        radius: Theme.panelRadius
        color: Theme.surface
        border.color: Theme.border
        HoverHandler {
            blocking: false
            cursorShape: Qt.ArrowCursor
        }
        // Keep clicks inside the floating card from reaching the video gestures.
        MouseArea {
            anchors.fill: parent
        }
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
                        objectName: "wordHeadword"
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
                    objectName: "wordGrammar"
                    visible: text.length > 0
                    text: stage.session.study.words.partOfSpeech
                    textFormat: Text.PlainText
                    color: Theme.muted
                    font.pixelSize: Theme.caption
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
                ActionButton {
                    objectName: "wordDetailsButton"
                    text: wordCard.detailsExpanded ? "Less" : "Details"
                    visible: stage.session.study.words.state === "complete"
                    checkable: true
                    checked: wordCard.detailsExpanded
                    Accessible.name: wordCard.detailsExpanded ? "Hide word details" : "Show word details"
                    onClicked: {
                        wordCard.detailsExpanded = !wordCard.detailsExpanded;
                        if (wordCard.detailsExpanded) stage.session.study.words.loadDetails();
                    }
                }
                ColumnLayout {
                    visible: wordCard.detailsExpanded
                    Layout.fillWidth: true
                    spacing: Theme.space2
                    Label {
                        text: stage.session.study.words.detailsState === "loading" ? "Loading details…"
                            : stage.session.study.words.detailsState === "failed" ? stage.session.study.words.detailsMessage
                            : !wordCard.hasDetails ? "No additional details for this word." : ""
                        visible: text.length > 0
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        text: "Try again"
                        visible: stage.session.study.words.detailsState === "failed"
                        onClicked: stage.session.study.words.loadDetails()
                    }
                    Label {
                        objectName: "wordFormNote"
                        font.pixelSize: Theme.caption
                        text: stage.session.study.words.formNote
                        visible: text.length > 0
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        Layout.fillWidth: true
                    }
                    Label {
                        text: stage.session.study.words.contextNote
                        visible: text.length > 0
                        textFormat: Text.PlainText
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        Layout.fillWidth: true
                    }
                    WordUsageSection {
                        objectName: "wordOtherSenses"
                        title: stage.session.study.words.lemma.length > 0 ? "Other senses of “" + stage.session.study.words.lemma + "”" : "Other senses"
                        entries: stage.session.study.words.otherSenses
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.space2
                    }
                    WordUsageSection {
                        objectName: "wordOtherUses"
                        title: "Other uses"
                        entries: stage.session.study.words.otherUses
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.space2
                    }
                }
                Label {
                    visible: stage.session.study.words.state !== "complete"
                    text: stage.session.study.words.state === "loading" ? "Looking up…" : stage.session.study.words.message
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    color: Theme.muted
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: "Try again"
                    visible: stage.session.study.words.state === "failed"
                    onClicked: stage.session.retryWordLookup()
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
