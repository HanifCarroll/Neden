pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: details
    required property var information
    readonly property var video: information.Video || ({})
    readonly property var audio: information.Audio || ({})
    function readable(value) {
        return value === undefined || value === null || value === "" ? "Unavailable" : String(value);
    }
    readonly property var sections: [
        {
            title: "Video",
            rows: [
                {
                    label: "Codec",
                    value: readable(information["Video codec"])
                },
                {
                    label: "Resolution",
                    value: video.w && video.h ? video.w + " × " + video.h + " px" : "Unavailable"
                },
                {
                    label: "Pixel format",
                    value: readable(video.pixelformat)
                },
                {
                    label: "Color space",
                    value: readable(video.colormatrix)
                },
                {
                    label: "Color transfer",
                    value: readable(video.gamma)
                },
                {
                    label: "Decoding",
                    value: !information["Hardware decoder"] ? "Unavailable" : information["Hardware decoder"] === "no" ? "Software" : "Hardware · " + information["Hardware decoder"]
                }
            ]
        },
        {
            title: "Audio",
            rows: [
                {
                    label: "Codec",
                    value: readable(information["Audio codec"])
                },
                {
                    label: "Channels",
                    value: readable(audio.channels)
                },
                {
                    label: "Sample rate",
                    value: audio.samplerate ? (audio.samplerate / 1000).toLocaleString(Qt.locale(), 'f', audio.samplerate % 1000 ? 1 : 0) + " kHz" : "Unavailable"
                }
            ]
        }
    ]
    spacing: Theme.space5
    Repeater {
        model: details.sections
        delegate: SettingsGroup {
            id: section
            required property var modelData
            title: modelData.title
            Layout.fillWidth: true
            Repeater {
                model: section.modelData.rows
                delegate: SettingsRow {
                    id: detailRow
                    required property var modelData
                    title: modelData.label
                    Label {
                        text: detailRow.modelData.value
                        textFormat: Text.PlainText
                        color: Theme.ink
                        font.pixelSize: Theme.body
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
