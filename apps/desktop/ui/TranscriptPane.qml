pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: pane
    required property LearningSession session
    readonly property bool editing: search.activeFocus
    property bool searchOpen: false
    property bool followingPlayback: true
    signal primaryRequested
    signal secondaryRequested

    function followCurrentCue() {
        if (!followingPlayback)
            return;

        const row = session.cueModel.rowForCue(session.activeCue);
        if (row >= 0) {
            transcript.currentIndex = row;
            transcript.forceLayout();
            transcript.positionViewAtIndex(row, ListView.Center);
            settleCurrentRow.restart();
        }
    }
    Timer {
        id: settleCurrentRow
        interval: 80
        onTriggered: {
            if (pane.followingPlayback && transcript.currentIndex >= 0) {
                transcript.forceLayout();
                transcript.positionViewAtIndex(transcript.currentIndex, ListView.Center);
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.leftMargin: Theme.space4
            Layout.rightMargin: Theme.space3
            Label {
                text: "Transcript"
                color: Theme.ink
                font.pixelSize: Theme.title
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            IconButton {
                visible: !pane.followingPlayback
                name: "captions"
                description: "Follow playback"
                onClicked: {
                    pane.followingPlayback = true;
                    pane.followCurrentCue();
                }
            }
            IconButton {
                objectName: "transcriptSearchButton"
                name: "search"
                description: "Search transcript"
                checked: pane.searchOpen
                onClicked: {
                    pane.searchOpen = !pane.searchOpen;
                    if (pane.searchOpen)
                        search.forceActiveFocus();
                    else {
                        search.text = "";
                        pane.forceActiveFocus();
                    }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space4
            Layout.rightMargin: Theme.space4
            Layout.bottomMargin: Theme.space2
            spacing: 0
            SubtitleTrackPicker {
                Layout.fillWidth: true
                session: pane.session
                onFileRequested: pane.primaryRequested()
                onFinished: pane.forceActiveFocus()
            }
            SubtitleTrackPicker {
                Layout.fillWidth: true
                session: pane.session
                secondary: true
                onFileRequested: pane.secondaryRequested()
                onFinished: pane.forceActiveFocus()
            }
        }
        ColumnLayout {
            visible: pane.searchOpen
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space4
            Layout.rightMargin: Theme.space4
            Layout.bottomMargin: Theme.space3
            spacing: Theme.space2
            SearchField {
                id: search
                objectName: "transcriptSearchField"
                Layout.fillWidth: true
                placeholderText: "Find a word or phrase…"
                onTextChanged: pane.session.cueModel.search(text)
                Accessible.name: "Search transcript"
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space4
            Layout.rightMargin: Theme.space4
            implicitHeight: 1
            color: Theme.border
        }
        ListView {
            id: transcript
            objectName: "transcriptList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: pane.session.cueModel
            reuseItems: true
            onMovementStarted: {
                pane.followingPlayback = false;
                settleCurrentRow.stop();
            }
            Component.onCompleted: Qt.callLater(pane.followCurrentCue)
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: cue
                required property string cueText
                required property string translation
                required property real startSeconds
                required property int cueIndex
                readonly property bool current: pane.session.activeCue === cue.cueIndex
                width: transcript.width
                height: cueContents.implicitHeight + 32
                padding: 0
                background: Rectangle {
                    color: cue.current ? Theme.selected : cue.hovered ? Theme.elevated : "transparent"
                    Rectangle {
                        width: 3
                        height: parent.height
                        color: Theme.accent
                        visible: cue.current
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        height: 1
                        color: Theme.border
                        opacity: 0.6
                    }
                }
                Accessible.name: cue.cueText
                onClicked: {
                    pane.followingPlayback = true;
                    pane.session.seekSubtitle(startSeconds);
                }
                RowLayout {
                    id: cueContents
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 14
                    Label {
                        text: Theme.clock(cue.startSeconds)
                        color: cue.current ? Theme.accent : Theme.muted
                        font.pixelSize: Theme.caption
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 44
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label {
                            Layout.fillWidth: true
                            text: cue.cueText
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            color: Theme.ink
                            font.pixelSize: Theme.body
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: cue.translation
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            color: Theme.muted
                            font.pixelSize: Theme.body
                        }
                    }
                }
            }
        }
        Label {
            visible: pane.session.cueModel.count === 0
            text: pane.searchOpen && search.text.length ? "No matching subtitles." : "Load primary subtitles to follow along. Add a secondary track to see both languages."
            color: Theme.muted
            font.pixelSize: Theme.body
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.margins: Theme.space4
        }
    }
    Connections {
        target: pane.session.cueModel
        function onCountChanged() {
            Qt.callLater(pane.followCurrentCue);
        }
    }
    Connections {
        target: pane.session
        function onCueChanged() {
            Qt.callLater(pane.followCurrentCue);
        }
    }
}
