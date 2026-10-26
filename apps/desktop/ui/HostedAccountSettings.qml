pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: pane
    required property HostedAccount account
    spacing: Theme.space3
    Label {
        text: "AI included · Invited preview · Test billing"
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    Flow {
        Layout.fillWidth: true
        spacing: Theme.space2
        ActionButton {
            text: "Sign in"
            primary: true
            visible: !pane.account.connected
            enabled: !pane.account.busy
            onClicked: pane.account.signIn()
        }
        ActionButton {
            text: "Account & billing"
            visible: pane.account.connected
            onClicked: pane.account.manageAccount()
        }
    }
    Label {
        text: pane.account.message
        color: Theme.muted
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        Layout.fillWidth: true
    }
    Label {
        visible: pane.account.connected
        text: {
            const allowance = pane.account.allowance;
            if (!allowance.words || !allowance.explanations)
                return "Checking allowance…";
            if (!allowance.canSubscribe)
                return "This preview is available to invited accounts only.";
            if (!allowance.subscribed)
                return "Subscribe on your account page to use hosted AI.";
            return allowance.words.used + " / " + allowance.words.limit + " word lookups · " + allowance.explanations.used + " / " + allowance.explanations.limit + " explanations and follow-ups";
        }
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    SettingsDisclosure {
        title: "Connection options"
        objectName: "cloudConnectionOptions"
        visible: !pane.account.connected && !pane.account.busy
        Layout.fillWidth: true
        ActionButton {
            text: "Use saved account"
            onClicked: pane.account.connectSaved()
        }
    }
    ActionButton {
        text: pane.account.connected ? "Sign out" : "Cancel connection"
        visible: pane.account.connected || pane.account.busy
        onClicked: pane.account.signOut()
    }
}
