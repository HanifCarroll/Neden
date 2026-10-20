pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: pane
    required property LearningSession session
    property alias page: categories.currentIndex
    enum Page {
        Audio,
        Picture,
        Subtitles,
        Queue,
        Information
    }
    signal addFiles
    signal loadSubtitles(bool secondary)
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space5
        SegmentBar {
            id: categories
            objectName: "playbackCategories"
            Layout.fillWidth: true
            SegmentButton {
                width: categories.availableWidth / Math.max(1, categories.count)
                text: "Audio"
            }
            SegmentButton {
                width: categories.availableWidth / Math.max(1, categories.count)
                text: "Picture"
            }
            SegmentButton {
                width: categories.availableWidth / Math.max(1, categories.count)
                text: "Subtitles"
            }
            SegmentButton {
                width: categories.availableWidth / Math.max(1, categories.count)
                text: "Queue"
            }
            SegmentButton {
                width: categories.availableWidth / Math.max(1, categories.count)
                text: "File info"
            }
            onCurrentIndexChanged: optionsFlickable.contentY = 0
        }
        ScrollView {
            id: optionsScroll
            Layout.fillHeight: true
            Layout.fillWidth: true
            contentWidth: availableWidth
            clip: true
            Flickable {
                id: optionsFlickable
                contentHeight: optionsContent.implicitHeight
                contentWidth: width
                boundsBehavior: Flickable.StopAtBounds
                ColumnLayout {
                    id: optionsContent
                    width: optionsScroll.availableWidth
                    spacing: Theme.space5
                    ColumnLayout {
                        visible: pane.page === PlayerOptions.Audio
                        Layout.fillWidth: true
                        spacing: Theme.space5
                        SettingsGroup {
                            title: "Sound"
                            Layout.fillWidth: true
                            SettingsRow {
                                title: "Audio track"
                                ChoiceBox {
                                    objectName: "audioTrackChoice"
                                    Layout.fillWidth: true
                                    textRole: "label"
                                    valueRole: "id"
                                    model: [
                                        {
                                            label: "Off",
                                            id: "no"
                                        }
                                    ].concat(pane.session.mediaTracks.filter(track => track.type === "audio"))
                                    currentIndex: {
                                        for (let index = 1; index < count; ++index)
                                            if (model[index].selected)
                                                return index;
                                        return 0;
                                    }
                                    Accessible.name: "Audio track"
                                    onActivated: pane.session.preferences.chooseAudio(String(currentValue))
                                }
                            }
                            SettingsRow {
                                title: "Output device"
                                ChoiceBox {
                                    Layout.fillWidth: true
                                    model: pane.session.audioDevices
                                    textRole: "description"
                                    valueRole: "name"
                                    currentIndex: Math.max(0, indexOfValue(pane.session.preferences.audioDevice))
                                    onActivated: pane.session.preferences.audioDevice = currentValue
                                    Accessible.name: "Audio output device"
                                }
                            }
                            SettingsRow {
                                title: "Night mode"
                                description: "Soften loud sounds."
                                Switch {
                                    checked: pane.session.preferences.nightMode
                                    onToggled: pane.session.preferences.nightMode = checked
                                    Accessible.name: "Night mode"
                                }
                            }
                        }
                        SettingsGroup {
                            title: "Synchronization"
                            description: "Positive values play audio later; negative values play it earlier."
                            Layout.fillWidth: true
                            SettingsRow {
                                title: "Audio delay"
                                TimingControl {
                                    Layout.fillWidth: true
                                    seconds: pane.session.preferences.audioDelay
                                    description: "Audio delay"
                                    onChanged: seconds => pane.session.preferences.audioDelay = seconds
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        visible: pane.page === PlayerOptions.Picture
                        Layout.fillWidth: true
                        spacing: Theme.space5
                        SettingsGroup {
                            title: "Size and orientation"
                            Layout.fillWidth: true
                            SettingsRow {
                                title: "Aspect ratio"
                                ChoiceBox {
                                    Layout.fillWidth: true
                                    model: ["Original", "16:9", "4:3", "1:1", "2.35:1"]
                                    currentIndex: Math.max(0, ["-1", "16:9", "4:3", "1:1", "2.35:1"].indexOf(pane.session.preferences.aspect))
                                    onActivated: pane.session.preferences.aspect = ["-1", "16:9", "4:3", "1:1", "2.35:1"][currentIndex]
                                    Accessible.name: "Picture aspect ratio"
                                }
                            }
                            SettingsRow {
                                title: "Rotation"
                                ActionButton {
                                    text: "Left 90°"
                                    onClicked: pane.session.preferences.rotation -= 90
                                }
                                Label {
                                    text: pane.session.preferences.rotation + "°"
                                    color: Theme.ink
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.preferredWidth: 48
                                }
                                ActionButton {
                                    objectName: "rotateRightButton"
                                    text: "Right 90°"
                                    onClicked: pane.session.preferences.rotation += 90
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                            SettingsRow {
                                title: "Fill window"
                                description: "Crop edges to fill the player."
                                Switch {
                                    checked: pane.session.preferences.fill
                                    onToggled: pane.session.preferences.fill = checked
                                    Accessible.name: "Fill window"
                                }
                            }
                        }
                        SettingsGroup {
                            title: "Processing"
                            Layout.fillWidth: true
                            SettingsRow {
                                title: "Deinterlace"
                                description: "Smooth comb-like edges in interlaced video."
                                Switch {
                                    checked: pane.session.preferences.deinterlace
                                    onToggled: pane.session.preferences.deinterlace = checked
                                    Accessible.name: "Deinterlace"
                                }
                            }
                        }
                        ActionButton {
                            objectName: "resetPictureButton"
                            text: "Reset picture adjustments"
                            onClicked: pane.session.preferences.resetPicture()
                        }
                    }
                    ColumnLayout {
                        visible: pane.page === PlayerOptions.Subtitles
                        Layout.fillWidth: true
                        spacing: Theme.space5
                        Repeater {
                            model: [false, true]
                            delegate: SettingsGroup {
                                id: subtitleGroup
                                required property bool modelData
                                title: modelData ? "Secondary subtitles" : "Primary subtitles"
                                Layout.fillWidth: true
                                SettingsRow {
                                    title: "Track"
                                    ChoiceBox {
                                        objectName: subtitleGroup.modelData ? "secondaryOptionsTrackChoice" : "primaryOptionsTrackChoice"
                                        Layout.fillWidth: true
                                        textRole: "name"
                                        model: [
                                            {
                                                name: "Off",
                                                path: ""
                                            }
                                        ].concat(pane.session.subtitleTracks.filter(track => subtitleGroup.modelData ? !track.primary : !track.secondary))
                                        currentIndex: {
                                            for (let index = 1; index < count; ++index)
                                                if (subtitleGroup.modelData ? model[index].secondary : model[index].primary)
                                                    return index;
                                            return 0;
                                        }
                                        Accessible.name: subtitleGroup.title
                                        onActivated: {
                                            if (currentIndex === 0)
                                                pane.session.clearTrack(subtitleGroup.modelData);
                                            else
                                                pane.session.chooseTrack(model[currentIndex].path, subtitleGroup.modelData);
                                        }
                                    }
                                    ActionButton {
                                        text: "Open…"
                                        Accessible.name: "Open " + subtitleGroup.title.toLowerCase()
                                        onClicked: pane.loadSubtitles(subtitleGroup.modelData)
                                    }
                                }
                                SettingsRow {
                                    title: "Delay"
                                    TimingControl {
                                        Layout.fillWidth: true
                                        seconds: subtitleGroup.modelData ? pane.session.secondaryDelay : pane.session.primaryDelay
                                        description: subtitleGroup.title + " delay"
                                        onChanged: seconds => pane.session.setSubtitleDelay(seconds, subtitleGroup.modelData)
                                    }
                                }
                            }
                        }
                        Label {
                            text: pane.session.independentNativeSubtitleTiming ? "Positive delay shows subtitles later." : "Positive delay shows subtitles later. Timing is shared when both tracks use embedded or styled subtitles. SRT and VTT files can be adjusted separately."
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.caption
                            Layout.fillWidth: true
                            color: Theme.muted
                        }
                        SettingsGroup {
                            title: "Subtitle files"
                            Layout.fillWidth: true
                            SettingsRow {
                                title: "Text encoding"
                                description: "For files opened next."
                                ChoiceBox {
                                    Layout.fillWidth: true
                                    model: ["Automatic", "UTF-8", "UTF-16", "windows-1254", "windows-1252", "ISO-8859-9"]
                                    currentIndex: Math.max(0, ["auto", "UTF-8", "UTF-16", "windows-1254", "windows-1252", "ISO-8859-9"].indexOf(pane.session.preferences.subtitleEncoding))
                                    onActivated: pane.session.preferences.subtitleEncoding = currentIndex === 0 ? "auto" : currentText
                                    Accessible.name: "Subtitle file encoding"
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        visible: pane.page === PlayerOptions.Queue
                        Layout.fillWidth: true
                        spacing: Theme.space4
                        RowLayout {
                            Layout.fillWidth: true
                            ActionButton {
                                text: "Add files…"
                                onClicked: pane.addFiles()
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            ActionButton {
                                text: "Clear queue"
                                flat: true
                                enabled: pane.session.queue.entries.length > 0
                                onClicked: pane.session.queue.clear()
                            }
                        }
                        CheckBox {
                            text: "Play the next video automatically"
                            checked: pane.session.queue.autoplay
                            onToggled: pane.session.queue.autoplay = checked
                        }
                        Label {
                            visible: pane.session.queue.entries.length === 0
                            text: "Your queue is empty. Add videos to play them in order."
                            color: Theme.muted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Repeater {
                            model: pane.session.queue.entries
                            delegate: Frame {
                                id: queueRow
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                padding: Theme.space2
                                background: Rectangle {
                                    color: queueRow.modelData.current ? Theme.selected : "transparent"
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: 1
                                        color: Theme.border
                                    }
                                }
                                contentItem: RowLayout {
                                    IconButton {
                                        name: "play"
                                        description: "Play " + queueRow.modelData.title
                                        checked: queueRow.modelData.current
                                        onClicked: pane.session.queue.play(queueRow.index)
                                    }
                                    Label {
                                        text: queueRow.modelData.title
                                        color: Theme.ink
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                    ActionButton {
                                        text: "↑"
                                        implicitWidth: Theme.controlHeight
                                        enabled: queueRow.index > 0
                                        Accessible.name: "Move video earlier"
                                        onClicked: pane.session.queue.move(queueRow.index, queueRow.index - 1)
                                    }
                                    ActionButton {
                                        text: "↓"
                                        implicitWidth: Theme.controlHeight
                                        enabled: queueRow.index + 1 < pane.session.queue.entries.length
                                        Accessible.name: "Move video later"
                                        onClicked: pane.session.queue.move(queueRow.index, queueRow.index + 1)
                                    }
                                    IconButton {
                                        name: "x"
                                        description: "Remove " + queueRow.modelData.title + " from queue"
                                        onClicked: pane.session.queue.remove(queueRow.index)
                                    }
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        visible: pane.page === PlayerOptions.Information
                        Layout.fillWidth: true
                        spacing: Theme.space5
                        MediaDetails {
                            Layout.fillWidth: true
                            information: pane.session.mediaInformation
                        }
                        ActionButton {
                            text: "Show file in folder"
                            onClicked: pane.session.revealFile()
                        }
                    }
                }
            }
        }
    }
}
