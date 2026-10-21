pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Neden

Item {
    id: dialogs
    objectName: "playerDialogs"
    required property LearningSession session
    required property RecentVideos recentVideos
    required property ShortcutMap keymap
    required property SubtitleAppearance appearance
    required property FullscreenController fullscreenController
    property bool appendToQueue: false
    property bool screenshotWithSubtitles: true
    property int settingsPage: 0
    readonly property bool settingsOpen: settingsDialog.opened
    readonly property bool blocksShortcuts: settingsDialog.opened || playerOptionsDialog.opened || timeDialog.opened || urlDialog.opened
    signal focusRequested
    signal subtitleScreenshotRequested(string path)

    function openVideo(append = false) {
        appendToQueue = append;
        videoDialog.open();
    }
    function openFolder() {
        folderDialog.open();
    }
    function openUrl() {
        urlDialog.open();
    }
    function jumpToTime() {
        timeDialog.open();
    }
    function openPrimarySubtitles() {
        subtitleDialog.open();
    }
    function openSecondarySubtitles() {
        translationDialog.open();
    }
    function locateVideo(url) {
        locateDialog.oldUrl = url;
        locateDialog.open();
    }
    function saveScreenshot(withSubtitles) {
        screenshotWithSubtitles = withSubtitles;
        screenshotDialog.open();
    }
    function openPlayerOptions(page) {
        playerOptions.page = page;
        playerOptionsDialog.open();
    }
    function openSettings(page) {
        if (page !== undefined)
            settingsPage = page;
        settingsLoader.active = true;
        settingsDialog.open();
    }
    function closeSettings() {
        settingsDialog.close();
    }
    FileDialog {
        id: videoDialog
        objectName: "videoPicker"
        title: dialogs.appendToQueue ? "Add videos to queue" : "Open videos"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Video files (*.mp4 *.mkv *.webm *.mov *.m4v *.avi *.mpg *.mpeg *.ts *.wmv)", "All files (*)"]
        onAccepted: {
            if (dialogs.appendToQueue)
                dialogs.session.queue.append(selectedFiles);
            else
                dialogs.session.openVideos(selectedFiles);
            dialogs.appendToQueue = false;
        }
        onRejected: dialogs.appendToQueue = false
    }
    FileDialog {
        id: subtitleDialog
        objectName: "primaryPicker"
        title: "Choose Primary subtitles"
        nameFilters: ["Subtitle files (*.srt *.vtt *.ass *.ssa *.sup *.sub *.idx)"]
        onAccepted: dialogs.session.openSubtitles(selectedFile)
    }

    FileDialog {
        id: translationDialog
        objectName: "secondaryPicker"
        title: "Choose Secondary subtitles"
        nameFilters: ["Subtitle files (*.srt *.vtt *.ass *.ssa *.sup *.sub *.idx)"]
        onAccepted: dialogs.session.openTranslation(selectedFile)
    }
    FileDialog {
        id: locateDialog
        property url oldUrl
        title: "Locate moved video"
        onAccepted: dialogs.recentVideos.relocate(oldUrl, selectedFile)
    }
    FolderDialog {
        id: folderDialog
        title: "Open video folder"
        onAccepted: dialogs.session.openVideos([selectedFolder])
    }
    FileDialog {
        id: screenshotDialog
        title: "Save screenshot"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG image (*.png)"]
        defaultSuffix: "png"
        onAccepted: {
            if (dialogs.screenshotWithSubtitles)
                dialogs.subtitleScreenshotRequested(dialogs.session.localFilePath(selectedFile));
            else
                dialogs.session.saveScreenshot(selectedFile);
        }
    }
    OptionsDialog {
        id: playerOptionsDialog
        objectName: "playerOptionsDialog"
        title: "Playback options"
        modal: true
        anchors.centerIn: parent
        width: Math.min(dialogs.width - 40, 820)
        height: Math.min(dialogs.height - 40, 660)
        contentItem: PlayerOptions {
            id: playerOptions
            objectName: "playerOptions"
            onLoadSubtitles: secondary => secondary ? translationDialog.open() : subtitleDialog.open()
            session: dialogs.session
            onAddFiles: {
                dialogs.appendToQueue = true;
                videoDialog.open();
            }
        }
        onClosed: dialogs.focusRequested()
    }
    Dialog {
        id: timeDialog
        objectName: "jumpToTimeDialog"
        title: "Jump to time"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Cancel
        ColumnLayout {
            SearchField {
                id: timeField
                placeholderText: "HH:MM:SS or seconds"
                Accessible.name: "Jump to time"
                onAccepted: jumpButton.clicked()
            }
            Label {
                id: timeError
                color: Theme.muted
                text: ""
            }
            ActionButton {
                id: jumpButton
                text: "Go"
                onClicked: {
                    if (dialogs.session.jumpToTime(timeField.text))
                        timeDialog.close();
                    else
                        timeError.text = "Enter a valid playback time.";
                }
            }
        }
        onOpened: timeField.forceActiveFocus()
        onClosed: dialogs.focusRequested()
    }
    Dialog {
        id: urlDialog
        objectName: "openUrlDialog"
        title: "Open media URL"
        modal: true
        anchors.centerIn: parent
        width: Math.min(dialogs.width - 40, 560)
        standardButtons: Dialog.Cancel
        ColumnLayout {
            width: parent.width
            SearchField {
                id: urlField
                Layout.fillWidth: true
                placeholderText: "https://…"
                Accessible.name: "Media URL"
            }
            Label {
                text: "Use a direct HTTP or HTTPS media URL."
                color: Theme.muted
            }
            ActionButton {
                text: "Open"
                onClicked: {
                    dialogs.session.openVideos([urlField.text]);
                    urlDialog.close();
                }
            }
        }
        onClosed: dialogs.focusRequested()
    }
    OptionsDialog {
        id: settingsDialog
        objectName: "settingsDialog"
        title: "Settings"
        anchors.centerIn: parent
        width: Math.min(dialogs.width - 48, 720)
        height: Math.min(dialogs.height - 48, 700)
        modal: true
        contentItem: Loader {
            id: settingsLoader
            objectName: "settingsLoader"
            active: false
            // Retain drafts after first use, without constructing controls at startup.
            sourceComponent: SettingsPane {
                keymap: dialogs.keymap
                page: dialogs.settingsPage
                onPageChanged: dialogs.settingsPage = page
                session: dialogs.session
                fullscreenController: dialogs.fullscreenController
                appearance: dialogs.appearance
            }
        }
        onClosed: {
            dialogs.appearance.cancelPreview();
            settingsLoader.active = false;
            dialogs.focusRequested();
        }
    }
}
