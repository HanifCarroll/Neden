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
    property string loadState: "idle"
    property string errorMessage: ""
    signal dismissed

    function lookup(value) {
        term = value.trim();
        if (!term.length)
            return;
        query.text = term;
        const address = pane.session.study.settings.dictionaryUrl(term);
        if (!address.toString().length) {
            errorMessage = "Set a valid dictionary URL in Settings.";
            loadState = "invalid";
            return;
        }
        browser.url = address;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space2
        RowLayout {
            Layout.leftMargin: Theme.space3
            Layout.rightMargin: Theme.space3
            Layout.topMargin: Theme.space3
            IconButton {
                objectName: "dictionaryBackButton"
                name: "arrow-left"
                description: "Back to previous dictionary page"
                visible: browser.canGoBack
                onClicked: browser.goBack()
            }
            Label {
                text: "Dictionary"
                color: Theme.ink
                font.bold: true
                Layout.fillWidth: true
            }
            BusyIndicator {
                running: pane.loadState === "loading"
                visible: running
                implicitWidth: 24
                implicitHeight: 24
            }
        }
        RowLayout {
            Layout.leftMargin: Theme.space3
            Layout.rightMargin: Theme.space3
            SearchField {
                id: query
                Layout.fillWidth: true
                placeholderText: "Search dictionary…"
                Accessible.name: "Dictionary word"
                onAccepted: pane.lookup(text)
            }
        }
        RowLayout {
            Layout.leftMargin: Theme.space3
            Layout.rightMargin: Theme.space3
            visible: pane.loadState === "failed" || pane.loadState === "invalid"
            Label {
                text: pane.errorMessage
                textFormat: Text.PlainText
                color: Theme.muted
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            ActionButton {
                objectName: "dictionaryRetryButton"
                text: "Retry"
                visible: pane.loadState === "failed"
                onClicked: browser.reload()
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
                    pane.errorMessage = "";
                    pane.loadState = "loading";
                } else if (request.status === WebView.LoadSucceededStatus) {
                    pane.loadState = "ready";
                } else if (request.status === WebView.LoadFailedStatus) {
                    pane.errorMessage = "Couldn’t load the dictionary.";
                    pane.loadState = "failed";
                }
            }
        }
    }
}
