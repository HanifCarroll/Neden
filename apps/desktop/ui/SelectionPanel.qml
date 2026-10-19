import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

Rectangle {
    id: panel
    required property LearningSession session
    signal openDictionary
    signal dismissed
    implicitHeight: contents.implicitHeight + 36
    color: Theme.surface
    radius: 14
    border.color: Theme.border
    ColumnLayout {
        id: contents
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14
        RowLayout {
            Label {
                Layout.fillWidth: true
                text: panel.session.selection
                color: Theme.accent
                font.pixelSize: 20
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
            ActionButton {
                flat: true
                text: "Close"
                onClicked: panel.dismissed()
            }
        }
        Label {
            Layout.fillWidth: true
            text: "Open Study for an AI explanation or translation. Dictionary opens the selected word in your configured website."
            color: Theme.ink
            wrapMode: Text.WordWrap
            font.pixelSize: 13
            textFormat: Text.PlainText
        }
        RowLayout {
            ActionButton {
                text: "Dictionary"
                onClicked: panel.openDictionary()
            }
            ActionButton {
                text: "Save moment"
                flat: true
                onClicked: panel.session.saveCard()
            }
        }
    }
}
