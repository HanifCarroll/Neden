pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: pane
    required property AppSettings settings
    required property string service
    property var prepareConnection: function () {
        return true;
    }
    spacing: Theme.space3
    SearchField {
        id: newKey
        text: ""
        echoMode: TextInput.Password
        maximumLength: 512
        placeholderText: "New API key"
        Layout.fillWidth: true
        Accessible.name: "New API key"
    }
    Flow {
        Layout.fillWidth: true
        spacing: Theme.space2
        ActionButton {
            text: "Save key"
            enabled: newKey.text.length > 0
            onClicked: {
                if (pane.service !== "ai" || pane.prepareConnection()) {
                    pane.settings.saveServiceKey(pane.service, newKey.text);
                    newKey.clear();
                }
            }
        }
        ActionButton {
            text: "Use saved key"
            onClicked: {
                if (pane.service !== "ai" || pane.prepareConnection())
                    pane.settings.connectServiceKey(pane.service);
            }
        }
    }
    Label {
        text: "Keys are stored securely on this device."
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
}
