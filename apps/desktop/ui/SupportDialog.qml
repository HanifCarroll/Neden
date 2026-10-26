pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

OptionsDialog {
    id: dialog
    objectName: "supportDialog"
    required property LearningSession session
    property bool updates: false
    title: updates ? "Updates" : "Report a problem"
    width: Math.min(parent.width - 40, 600)
    height: Math.min(parent.height - 40, 520)
    anchors.centerIn: parent
    ReleaseCheck {
        id: releases
    }
    SupportDiagnostics {
        id: diagnostics
    }
    onOpened: {
        copied.text = "";
        if (updates)
            releases.check();
    }
    contentItem: ScrollView {
        id: scroll
        contentWidth: availableWidth
        ColumnLayout {
            width: scroll.availableWidth
            spacing: Theme.space4
            Label {
                text: "Neden " + releases.currentVersion
                color: Theme.ink
                font.pixelSize: Theme.title
            }
            Label {
                visible: dialog.updates
                text: releases.message
                color: Theme.ink
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Accessible.role: Accessible.StaticText
            }
            Label {
                visible: dialog.updates
                text: "Checks run only when you ask. GitHub receives a normal web request, including your IP address; no profile or media information is sent."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ActionButton {
                visible: dialog.updates
                text: releases.releaseUrl.toString().length ? "View update and release notes" : "View all releases"
                onClicked: Qt.openUrlExternally(releases.releaseUrl.toString().length ? releases.releaseUrl : "https://github.com/HanifCarroll/Neden/releases")
            }
            ActionButton {
                visible: dialog.updates
                text: "Check again"
                enabled: !releases.checking
                onClicked: releases.check()
            }
            Label {
                visible: !dialog.updates
                text: "Describe what you expected, what happened, and the steps to reproduce it. You can copy these technical details into your report. Nothing is uploaded automatically."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            TextArea {
                visible: !dialog.updates
                readOnly: true
                selectByMouse: true
                text: diagnostics.report(dialog.session.hasMedia, dialog.session.paused, dialog.session.mediaInformation)
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                Accessible.name: "Technical details for a bug report"
                color: Theme.ink
                background: null
                Layout.fillWidth: true
            }
            ActionButton {
                objectName: "copyDiagnosticsButton"
                visible: !dialog.updates
                text: "Copy diagnostics"
                onClicked: {
                    diagnostics.copy(dialog.session.hasMedia, dialog.session.paused, dialog.session.mediaInformation);
                    copied.text = "Copied to clipboard.";
                }
            }
            Label {
                id: copied
                visible: text.length > 0 && !dialog.updates
                color: Theme.muted
            }
            ActionButton {
                visible: !dialog.updates
                text: "Open bug report…"
                onClicked: Qt.openUrlExternally("https://github.com/HanifCarroll/Neden/issues/new?template=bug_report.md")
            }
        }
    }
}
