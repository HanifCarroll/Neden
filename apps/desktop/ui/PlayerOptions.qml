pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: pane
    required property LearningSession session
    property int page: 0
    signal addFiles
    signal locateFile(url oldUrl)
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space4
        RowLayout {
            Layout.fillWidth: true
            Repeater {
                model: ["Audio", "Picture", "Subtitles", "Queue", "Information", "Preferences"]
                delegate: InspectorTab {
                    required property string modelData
                    required property int index
                    text: modelData
                    checked: pane.page === index
                    Layout.fillWidth: true
                    onClicked: pane.page = index
                }
            }
        }
        ScrollView {
            Layout.fillHeight: true
            Layout.fillWidth: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: Theme.space4
                ColumnLayout {
                    visible: pane.page === 0
                    Layout.fillWidth: true
                    Label {
                        text: "Audio track"
                        color: Theme.ink
                    }
                    Repeater {
                        model: pane.session.mediaTracks.filter(track => track.type === "audio")
                        delegate: RadioButton {
                            required property var modelData
                            text: modelData.label
                            checked: modelData.selected === true
                            onClicked: pane.session.preferences.chooseAudio(String(modelData.id))
                        }
                    }
                    ActionButton {
                        text: "Disable audio"
                        onClicked: pane.session.preferences.chooseAudio("no")
                    }
                    Label {
                        text: "Output device"
                        color: Theme.ink
                    }
                    ChoiceBox {
                        Layout.fillWidth: true
                        model: pane.session.audioDevices
                        textRole: "description"
                        valueRole: "name"
                        currentIndex: Math.max(0, indexOfValue(pane.session.preferences.audioDevice))
                        onActivated: pane.session.preferences.audioDevice = currentValue
                        Accessible.name: "Audio output device"
                    }
                    Label {
                        text: "Audio delay · positive values play audio later"
                        color: Theme.muted
                    }
                    RowLayout {
                        ActionButton {
                            text: "−0.1 s"
                            onClicked: pane.session.preferences.audioDelay -= 0.1
                        }
                        Label {
                            text: pane.session.preferences.audioDelay.toFixed(1) + " s"
                            color: Theme.ink
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                        ActionButton {
                            text: "+0.1 s"
                            onClicked: pane.session.preferences.audioDelay += 0.1
                        }
                        ActionButton {
                            text: "Reset"
                            onClicked: pane.session.preferences.audioDelay = 0
                        }
                    }
                    CheckBox {
                        text: "Night mode · reduce loud sound peaks"
                        checked: pane.session.preferences.nightMode
                        onToggled: pane.session.preferences.nightMode = checked
                    }
                }
                ColumnLayout {
                    visible: pane.page === 1
                    Layout.fillWidth: true
                    Label {
                        text: "Rotation"
                        color: Theme.ink
                    }
                    RowLayout {
                        ActionButton {
                            text: "Rotate left"
                            onClicked: pane.session.preferences.rotation -= 90
                        }
                        Label {
                            text: pane.session.preferences.rotation + "°"
                            color: Theme.ink
                        }
                        ActionButton {
                            text: "Rotate right"
                            onClicked: pane.session.preferences.rotation += 90
                        }
                    }
                    Label {
                        text: "Aspect ratio"
                        color: Theme.ink
                    }
                    ChoiceBox {
                        model: ["Original", "16:9", "4:3", "1:1", "2.35:1"]
                        currentIndex: Math.max(0, ["-1", "16:9", "4:3", "1:1", "2.35:1"].indexOf(pane.session.preferences.aspect))
                        onActivated: pane.session.preferences.aspect = ["-1", "16:9", "4:3", "1:1", "2.35:1"][currentIndex]
                        Accessible.name: "Picture aspect ratio"
                    }
                    CheckBox {
                        text: "Fill window · crop edges"
                        checked: pane.session.preferences.fill
                        onToggled: pane.session.preferences.fill = checked
                    }
                    CheckBox {
                        text: "Deinterlace"
                        checked: pane.session.preferences.deinterlace
                        onToggled: pane.session.preferences.deinterlace = checked
                    }
                    ActionButton {
                        text: "Reset picture"
                        onClicked: pane.session.preferences.resetPicture()
                    }
                }
                ColumnLayout {
                    visible: pane.page === 2
                    Layout.fillWidth: true
                    Label {
                        text: "Embedded and styled tracks"
                        color: Theme.ink
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "Choose a track for either subtitle position. Primary styled tracks retain their appearance. Secondary styling and bitmap placement follow mpv; bitmap tracks can overlap."
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Repeater {
                        model: pane.session.mediaTracks.filter(track => track.type === "sub")
                        delegate: RowLayout {
                            id: subtitleTrackRow
                            required property var modelData
                            Layout.fillWidth: true
                            Label {
                                text: subtitleTrackRow.modelData.label
                                color: Theme.ink
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            ActionButton {
                                text: "Primary"
                                onClicked: pane.session.selectNativeSubtitle(String(subtitleTrackRow.modelData.id), false)
                            }
                            ActionButton {
                                text: "Secondary"
                                onClicked: pane.session.selectNativeSubtitle(String(subtitleTrackRow.modelData.id), true)
                            }
                        }
                    }
                    Repeater {
                        model: [false, true]
                        delegate: ColumnLayout {
                            id: delayRow
                            required property bool modelData
                            Label {
                                text: delayRow.modelData ? "Secondary delay" : "Primary delay"
                                color: Theme.ink
                            }
                            RowLayout {
                                ActionButton {
                                    text: "−0.1 s"
                                    onClicked: pane.session.setSubtitleDelay((delayRow.modelData ? pane.session.secondaryDelay : pane.session.primaryDelay) - 0.1, delayRow.modelData)
                                }
                                Label {
                                    text: (delayRow.modelData ? pane.session.secondaryDelay : pane.session.primaryDelay).toFixed(1) + " s"
                                    color: Theme.ink
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                ActionButton {
                                    text: "+0.1 s"
                                    onClicked: pane.session.setSubtitleDelay((delayRow.modelData ? pane.session.secondaryDelay : pane.session.primaryDelay) + 0.1, delayRow.modelData)
                                }
                                ActionButton {
                                    text: "Reset"
                                    onClicked: pane.session.setSubtitleDelay(0, delayRow.modelData)
                                }
                                ActionButton {
                                    text: "Off"
                                    onClicked: pane.session.clearTrack(delayRow.modelData)
                                }
                            }
                        }
                    }
                    Label {
                        text: pane.session.independentNativeSubtitleTiming ? "Positive delay shows subtitles later." : "SRT/VTT delays are independent. This playback runtime shares one delay between two native subtitle tracks."
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: Theme.muted
                    }
                    Label {
                        text: "Encoding for newly opened subtitle files"
                        color: Theme.ink
                    }
                    ChoiceBox {
                        model: ["auto", "UTF-8", "UTF-16", "windows-1254", "windows-1252", "ISO-8859-9"]
                        currentIndex: Math.max(0, model.indexOf(pane.session.preferences.subtitleEncoding))
                        onActivated: pane.session.preferences.subtitleEncoding = currentText
                        Accessible.name: "Subtitle file encoding"
                    }
                }
                ColumnLayout {
                    visible: pane.page === 3
                    Layout.fillWidth: true
                    RowLayout {
                        ActionButton {
                            text: "Add files…"
                            onClicked: pane.addFiles()
                        }
                        ActionButton {
                            text: "Clear queue"
                            onClicked: pane.session.queue.clear()
                        }
                    }
                    CheckBox {
                        text: "Play the next video automatically"
                        checked: pane.session.queue.autoplay
                        onToggled: pane.session.queue.autoplay = checked
                    }
                    Repeater {
                        model: pane.session.queue.entries
                        delegate: RowLayout {
                            id: queueRow
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            ActionButton {
                                text: queueRow.modelData.current ? "Playing" : "Play"
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
                                enabled: queueRow.index > 0
                                Accessible.name: "Move video earlier"
                                onClicked: pane.session.queue.move(queueRow.index, queueRow.index - 1)
                            }
                            ActionButton {
                                text: "↓"
                                enabled: queueRow.index + 1 < pane.session.queue.entries.length
                                Accessible.name: "Move video later"
                                onClicked: pane.session.queue.move(queueRow.index, queueRow.index + 1)
                            }
                            ActionButton {
                                text: "Remove"
                                onClicked: pane.session.queue.remove(queueRow.index)
                            }
                        }
                    }
                }
                ColumnLayout {
                    visible: pane.page === 4
                    Layout.fillWidth: true
                    Repeater {
                        model: Object.keys(pane.session.mediaInformation)
                        delegate: ColumnLayout {
                            id: informationRow
                            required property string modelData
                            Layout.fillWidth: true
                            Label {
                                text: informationRow.modelData
                                color: Theme.muted
                            }
                            Label {
                                text: typeof pane.session.mediaInformation[informationRow.modelData] === "object" ? JSON.stringify(pane.session.mediaInformation[informationRow.modelData], null, 2) : String(pane.session.mediaInformation[informationRow.modelData] || "Unavailable")
                                color: Theme.ink
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                            }
                        }
                    }
                    ActionButton {
                        text: "Show file in folder"
                        onClicked: pane.session.revealFile()
                    }
                }
                ColumnLayout {
                    visible: pane.page === 5
                    Layout.fillWidth: true
                    CheckBox {
                        text: "Remember playback speed between videos"
                        checked: pane.session.preferences.rememberSpeed
                        onToggled: pane.session.preferences.rememberSpeed = checked
                    }
                    CheckBox {
                        text: "Save recently played videos and positions"
                        checked: pane.session.preferences.rememberHistory
                        onToggled: pane.session.preferences.rememberHistory = checked
                    }
                    Label {
                        text: "Volume and output device are remembered. Network URLs are kept only in the current queue."
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        text: "Clear playback history"
                        onClicked: clearHistoryConfirmation.open()
                    }
                }
            }
        }
    }
    Dialog {
        id: clearHistoryConfirmation
        title: "Clear playback history?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            text: "Saved positions, subtitle choices and per-video settings will be removed."
            color: Theme.ink
        }
        onAccepted: pane.session.clearHistory()
    }
}
