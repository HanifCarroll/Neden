import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: header
    property bool hasMedia: false
    property bool showingRecent: true
    signal openRecent
    signal backToVideo
    signal openVideo
    implicitHeight: 56
    spacing: Theme.space3
    Rectangle {
        implicitWidth: 32
        implicitHeight: 32
        radius: Theme.controlRadius
        color: Theme.accent
        Text {
            anchors.centerIn: parent
            text: "n"
            color: Theme.background
            font.pixelSize: 24
            font.bold: true
        }
    }
    Label {
        text: "Neden"
        color: Theme.ink
        font.pixelSize: Theme.title
        font.weight: Font.DemiBold
    }
    Item {
        Layout.fillWidth: true
    }
    ActionButton {
        objectName: "recentVideosButton"
        text: header.showingRecent ? "Back to video" : "Recent videos"
        visible: header.hasMedia
        flat: true
        onClicked: {
            if (header.showingRecent)
                header.backToVideo();
            else
                header.openRecent();
        }
    }
    ActionButton {
        objectName: "openVideoButton"
        text: "Open video"
        onClicked: header.openVideo()
    }
}
