import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebView
import Neden

Rectangle {
    id: pane
    required property LearningSession session
    color: Theme.surface
    radius: Theme.panelRadius
    property string term: ""
    property string loadState: "Choose a word"
    signal dismissed

    function lookup(value) {
        term = value.trim();
        if (!term.length)
            return;
        query.text = term;
        browser.url = pane.session.study.dictionaryUrl(term);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space2
        RowLayout {
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 12
            Label {
                text: "Dictionary"
                color: Theme.ink
                font.bold: true
                Layout.fillWidth: true
            }
            ActionButton {
                text: "Save"
                onClicked: pane.session.saveCard()
            }
            ActionButton {
                text: "Close"
                onClicked: pane.dismissed()
            }
        }
        RowLayout {
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            SearchField {
                id: query
                Layout.fillWidth: true
                Accessible.name: "Dictionary word"
                onAccepted: pane.lookup(text)
            }
            ActionButton {
                text: "Look up"
                onClicked: pane.lookup(query.text)
            }
        }
        RowLayout {
            Layout.leftMargin: 12
            ActionButton {
                text: "Back"
                enabled: browser.canGoBack
                onClicked: browser.goBack()
            }
            ActionButton {
                text: "Reload"
                onClicked: {
                    browser.reload();
                }
            }
            Label {
                text: pane.loadState
                color: Theme.muted
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
        WebView {
            id: browser
            Layout.fillWidth: true
            Layout.fillHeight: true
            settings.localContentCanAccessFileUrls: false
            settings.localStorageEnabled: false
            onLoadingChanged: function (request) {
                if (request.status === WebView.LoadStartedStatus) {
                    pane.loadState = "Loading…";
                } else if (request.status === WebView.LoadSucceededStatus) {
                    pane.loadState = "Loaded";
                } else if (request.status === WebView.LoadFailedStatus) {
                    pane.loadState = "Page failed — try Reload";
                }
            }
        }
    }
}
