pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Neden

ApplicationWindow {
    id: root
    required property LearningSession session
    required property MpvPlayback playback
    required property FullscreenController fullscreenController
    property bool dictionaryOpen: false
    width: 1280
    height: 820
    minimumWidth: 1100
    minimumHeight: 640
    visible: true
    title: "Neden"
    color: Theme.background
    font.family: "Inter"
    font.pixelSize: 13
    palette.window: Theme.background
    palette.base: Theme.surface
    palette.text: Theme.ink
    palette.button: Theme.elevated
    palette.buttonText: Theme.ink
    palette.windowText: Theme.ink
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.background

    function openSettings() {
        if (studyDrawer.opened) studyDrawer.close();
        settingsLoader.active = true;
        settingsDialog.open();
    }
    function fullscreen() {
        fullscreenController.toggle();
    }
    function closeSelection() {
        session.closeDefinition();
        stage.deselect();
        contentItem.forceActiveFocus();
    }
    function openDictionary(term) {
        dictionaryOpen = true;
        const pane = dictionaryLoader.item as DictionaryPane;
        pane.lookup(term || session.selection);
    }
    function closeDictionary() {
        dictionaryOpen = false;
        closeSelection();
    }
    function escapeLayer() {
        if (settingsDialog.opened)
            settingsDialog.close();
        else if (studyDrawer.opened)
            studyDrawer.close();
        else if (dictionaryOpen)
            closeDictionary();
        else if (session.selection.length)
            closeSelection();
        else if (session.replaying)
            session.stopReplay();
        else if (fullscreenController.active)
            fullscreenController.setActive(false);
    }
    Shortcut {
        sequence: "Ctrl+," // Qt maps Control to Command on macOS.
        onActivated: root.openSettings()
    }
    Connections {
        target: root.fullscreenController
        function onSettingsRequested() { root.openSettings(); }
    }
    Shortcut {
        sequence: "F11"
        onActivated: root.fullscreen()
    }
    Shortcut {
        sequence: "Ctrl+Meta+F"
        onActivated: root.fullscreen()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: root.escapeLayer()
    }
    Shortcut {
        sequence: "Space"
        enabled: !settingsDialog.opened && !studyDrawer.opened && !root.dictionaryOpen && !transcript.editing && !stage.editing
        onActivated: root.session.toggle()
    }
    FileDialog {
        id: videoDialog
        title: "Open local video"
        onAccepted: root.session.openVideo(selectedFile)
    }
    FileDialog {
        id: subtitleDialog
        title: "Open SRT subtitles"
        nameFilters: ["SRT subtitles (*.srt)"]
        onAccepted: root.session.openSubtitles(selectedFile)
    }

    FileDialog {
        id: translationDialog
        title: "Open translation SRT subtitles"
        nameFilters: ["SRT subtitles (*.srt)"]
        onAccepted: root.session.openTranslation(selectedFile)
    }
    Dialog {
        id: settingsDialog
        objectName: "settingsDialog"
        title: "Settings"
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 640)
        height: Math.min(root.height - 48, 700)
        modal: true
        standardButtons: Dialog.Close
        contentItem: Loader {
            id: settingsLoader
            objectName: "settingsLoader"
            active: false
            // Retain drafts after first use, without constructing controls at startup.
            sourceComponent: SettingsPane {
                session: root.session
                fullscreenController: root.fullscreenController
            }
        }
        onClosed: root.contentItem.forceActiveFocus()
    }
    Drawer {
        id: studyDrawer
        objectName: "studyDrawer"
        edge: Qt.RightEdge
        width: Math.min(root.width - 40, 530)
        height: root.height
        modal: true
        onAboutToShow: studyLoader.active = true
        onClosed: { root.session.study.stop(); root.closeSelection(); }
        Loader {
            id: studyLoader
            objectName: "studyLoader"
            anchors.fill: parent
            active: false
            sourceComponent: StudyPane {
                session: root.session
                onDismissed: studyDrawer.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 8
        anchors.bottomMargin: 16
        spacing: 0
        PlayerHeader {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            onOpenSettings: root.openSettings()
            onOpenVideo: videoDialog.open()
            onOpenSubtitles: subtitleDialog.open()
            onOpenTranslation: translationDialog.open()
            onOpenStudy: {
                if (!root.session.selection.length && root.session.subtitle.length) root.session.select(root.session.subtitle);
                studyDrawer.open();
            }
            onOpenDictionary: root.openDictionary("")
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: 24
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 1
                color: Theme.border
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 24
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                Item {
                    implicitHeight: 8
                }
                RowLayout {
                    Label {
                        text: "Your listening space"
                        color: Theme.ink
                        font.pixelSize: 18
                        font.weight: Font.Medium
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Label {
                        text: "LOCAL SESSION"
                        color: Theme.muted
                        font.pixelSize: 10
                        font.letterSpacing: 1.4
                    }
                }
                Item {
                    implicitHeight: 18
                }
                VideoStage {
                    id: stage
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    session: root.session
                    playback: root.playback
                    dictionaryOpen: root.dictionaryOpen
                    onOpenDictionary: root.openDictionary("")
                    onCloseSelection: root.closeSelection()
                }
                PlaybackControls {
                    Layout.fillWidth: true
                    session: root.session
                    onFullscreen: root.fullscreen()
                }
                Item {
                    implicitHeight: 16
                }
                Label {
                    text: "Select a word to explore its meaning. Keep a moment to revisit later."
                    color: Theme.muted
                    font.pixelSize: 12
                }
            }
            TranscriptPane {
                id: transcript
                visible: !root.dictionaryOpen
                Layout.preferredWidth: 352
                Layout.fillHeight: true
                session: root.session
            }
            Loader {
                id: dictionaryLoader
                objectName: "dictionaryLoader"
                active: root.dictionaryOpen
                visible: active
                Layout.preferredWidth: 352
                Layout.fillHeight: true
                sourceComponent: DictionaryPane {
                    session: root.session
                    onDismissed: root.closeDictionary()
                }
            }
        }
        Item {
            implicitHeight: 18
        }
        RowLayout {
            Rectangle {
                implicitWidth: 6
                implicitHeight: 6
                radius: 3
                color: Theme.accent
            }
            Label {
                text: root.session.status
                color: Theme.muted
                font.pixelSize: 11
            }
            Item {
                Layout.fillWidth: true
            }
        }
    }
}
