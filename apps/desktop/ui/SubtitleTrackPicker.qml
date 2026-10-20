pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

RowLayout {
    id: picker
    required property LearningSession session
    property bool secondary: false
    signal fileRequested
    signal finished
    spacing: Theme.space2

    Label {
        text: picker.secondary ? "Secondary" : "Primary"
        color: Theme.muted
        font.pixelSize: Theme.caption
        Layout.preferredWidth: 68
    }
    ChoiceBox {
        id: choice
        objectName: picker.secondary ? "secondaryTrackChoice" : "primaryTrackChoice"
        Layout.fillWidth: true
        enabled: picker.session.hasMedia
        textRole: "name"
        readonly property int selectedTrackIndex: {
            for (let index = 1; index < model.length - 1; ++index) {
                if (picker.secondary ? model[index].secondary : model[index].primary)
                    return index;
            }
            return 0;
        }
        model: {
            const tracks = [{name: "Off", path: ""}];
            for (let index = 0; index < picker.session.subtitleTracks.length; ++index)
                tracks.push(picker.session.subtitleTracks[index]);
            tracks.push({name: "Load subtitle file…", path: ""});
            return tracks;
        }
        currentIndex: selectedTrackIndex
        displayText: selectedTrackIndex === 0 ? "Add subtitles…" : textAt(selectedTrackIndex)
        Accessible.name: (picker.secondary ? "Secondary subtitles: " : "Primary subtitles: ") + currentText
        ToolTip.visible: hovered && selectedTrackIndex > 0 && !popup.visible
        ToolTip.delay: 600
        ToolTip.text: currentText
        background: Rectangle {
            color: choice.hovered ? Theme.hover : "transparent"
            radius: Theme.controlRadius
            border.width: choice.activeFocus ? 1 : 0
            border.color: Theme.accent
        }
        onActivated: function(index) {
            if (index === count - 1)
                picker.fileRequested();
            else if (index === 0)
                picker.session.clearTrack(picker.secondary);
            else
                picker.session.chooseTrack(model[index].path, picker.secondary);
            // File selection may be cancelled. Always display the active track.
            currentIndex = Qt.binding(function() { return choice.selectedTrackIndex; });
            picker.finished();
        }
    }
}
