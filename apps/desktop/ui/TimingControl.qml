pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

RowLayout {
    id: timing
    required property real seconds
    required property string description
    signal changed(real seconds)
    spacing: Theme.space1
    ActionButton {
        text: "−"
        implicitWidth: Theme.controlHeight
        Accessible.name: timing.description + ": earlier by 0.1 seconds"
        onClicked: timing.changed(timing.seconds - 0.1)
    }
    Label {
        text: (timing.seconds > 0 ? "+" : "") + timing.seconds.toFixed(1) + " s"
        color: Theme.ink
        font.pixelSize: Theme.body
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
        Layout.minimumWidth: 65
        Accessible.name: timing.description + ": " + text
    }
    ActionButton {
        text: "+"
        implicitWidth: Theme.controlHeight
        Accessible.name: timing.description + ": later by 0.1 seconds"
        onClicked: timing.changed(timing.seconds + 0.1)
    }
    ActionButton {
        text: "Reset"
        flat: true
        enabled: Math.abs(timing.seconds) > 0.001
        onClicked: timing.changed(0)
    }
}
