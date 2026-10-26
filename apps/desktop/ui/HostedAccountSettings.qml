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
        text: "Neden Cloud · Preview"
        color: Theme.ink
        font.pixelSize: Theme.title
    }
    Label {
        text: "An invited preview with AI included. Sign in in your browser; your API keys stay separate. Billing is in test mode."
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    CheckBox {
        text: "Use Neden Cloud for explanations and word lookups"
        checked: pane.account.enabled
        onToggled: pane.account.enabled = checked
    }
    Flow {
        Layout.fillWidth: true
        spacing: Theme.space2
        ActionButton {
            text: "Sign in"
            enabled: !pane.account.busy
            onClicked: pane.account.signIn()
        }
        ActionButton {
            text: "Connect saved account"
            enabled: !pane.account.busy
            onClicked: pane.account.connectSaved()
        }
        ActionButton {
            text: "Account & billing"
            onClicked: pane.account.manageAccount()
        }
    }
    Label {
        text: (!pane.account.connected && !pane.account.busy ? "Not connected. " : "") + pane.account.message
        color: Theme.muted
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        Layout.fillWidth: true
    }
    Label {
        visible: pane.account.connected
        text: {
            const allowance = pane.account.allowance;
            if (!allowance.words || !allowance.explanations) return "Checking allowance…";
            if (!allowance.canSubscribe) return "This preview is available to invited accounts only.";
            if (!allowance.subscribed) return "Subscribe on your account page to use hosted AI.";
            return allowance.words.used + " / " + allowance.words.limit + " word lookups · "
                + allowance.explanations.used + " / " + allowance.explanations.limit + " explanations and follow-ups";
        }
        color: Theme.muted
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }
    ActionButton {
        text: pane.account.connected ? "Sign out" : "Cancel connection"
        visible: pane.account.connected || pane.account.busy
        onClicked: pane.account.signOut()
    }
}
