pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Item {
    id: editor
    required property ShortcutMap keymap
    onVisibleChanged: {
        if (!visible)
            keymap.cancelRecording();
    }
    Component.onDestruction: keymap.cancelRecording()
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space3
        Label {
            Layout.fillWidth: true
            text: "Click a shortcut to record a new combination. Changes are saved automatically."
            color: Theme.muted
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            SearchField {
                id: search
                objectName: "shortcutSearch"
                Layout.fillWidth: true
                placeholderText: "Find a command or shortcut…"
                Accessible.name: "Search shortcuts"
            }
            ActionButton {
                text: "Reset all…"
                onClicked: resetConfirmation.open()
            }
        }
        ColumnLayout {
            visible: editor.keymap.recording
            Layout.fillWidth: true
            spacing: Theme.space2
            Label {
                text: editor.keymap.recordingLabel
                color: Theme.ink
                font.weight: Font.DemiBold
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    objectName: "shortcutCandidate"
                    Layout.fillWidth: true
                    text: editor.keymap.candidateLabel
                    color: Theme.accent
                    font.pixelSize: Theme.title
                }
                ActionButton {
                    objectName: "saveShortcut"
                    text: "Save"
                    onClicked: editor.keymap.saveRecording()
                }
                ActionButton {
                    text: "Cancel"
                    flat: true
                    onClicked: editor.keymap.cancelRecording()
                }
            }
            Label {
                text: "Press Escape to cancel."
                color: Theme.muted
            }
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: editor.keymap.message
            color: Theme.muted
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            Accessible.role: Accessible.AlertMessage
        }
        ListView {
            id: list
            objectName: "shortcutList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: editor.keymap.commands.filter(command => {
                const query = search.text.trim().toLocaleLowerCase();
                return (command.name + " " + command.group + " " + command.display).toLocaleLowerCase().indexOf(query) !== -1;
            })
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                id: row
                required property var modelData
                width: list.width
                height: 56
                Rectangle {
                    width: parent.width
                    height: 1
                    color: Theme.border
                }
                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.space2
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        color: Theme.ink
                        elide: Text.ElideRight
                    }
                    ActionButton {
                        objectName: "shortcut_" + row.modelData.id
                        Layout.preferredWidth: 130
                        text: row.modelData.display
                        flat: true
                        Accessible.name: "Change shortcut for " + row.modelData.name
                        onClicked: editor.keymap.beginRecording(row.modelData.id)
                    }
                    IconButton {
                        name: "x"
                        description: "Clear shortcut for " + row.modelData.name
                        enabled: row.modelData.sequence.length > 0
                        onClicked: editor.keymap.assign(row.modelData.id, "")
                    }
                    ActionButton {
                        text: "Reset"
                        flat: true
                        enabled: row.modelData.modified
                        onClicked: editor.keymap.reset(row.modelData.id)
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: "No matching commands"
                color: Theme.muted
            }
        }
        Label {
            Layout.fillWidth: true
            text: Qt.platform.os === "osx" ? "Fn–F and Control–Command–F use macOS fullscreen. Standard editing, Settings, Close and Quit shortcuts stay fixed. macOS may reserve additional combinations." : "Standard editing, Settings, Close and Quit shortcuts stay fixed. Your operating system may reserve additional combinations."
            color: Theme.muted
            font.pixelSize: Theme.caption
            wrapMode: Text.WordWrap
        }
    }
    Dialog {
        id: resetConfirmation
        parent: Overlay.overlay
        title: "Reset all shortcuts?"
        width: Math.min(420, parent.width - 32)
        x: Math.max(0, (parent.width - width) / 2)
        y: Math.max(0, (parent.height - height) / 2)
        modal: true
        standardButtons: Dialog.Reset | Dialog.Cancel
        onReset: {
            editor.keymap.resetAll();
            close();
        }
        contentItem: Label {
            text: "Your custom shortcuts will be replaced with the defaults."
            wrapMode: Text.WordWrap
        }
    }
}
