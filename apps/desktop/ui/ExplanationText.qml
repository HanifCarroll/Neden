pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Neden

TextArea {
    id: answer
    property string markdown: ""
    property bool scrolling: false
    property string activeReference: ""
    readonly property string breakdownText: ExplanationFormat.breakdown(markdown, activeReference)
    readOnly: true
    hoverEnabled: true
    selectByMouse: true
    wrapMode: TextEdit.Wrap
    textFormat: TextEdit.RichText
    text: ExplanationFormat.render(markdown, Theme.ink)
    color: Theme.ink
    selectionColor: Theme.accent
    font.pixelSize: Theme.body
    padding: 0
    background: Item {}

    function dismissBreakdown() {
        revealDelay.stop();
        dismissDelay.stop();
        breakdownPopup.close();
        activeReference = "";
    }
    function revealBreakdown(link: string) {
        if (scrolling || !visible || !ExplanationFormat.breakdown(markdown, link).length)
            return;
        activeReference = link;
        const position = mapToItem(breakdownPopup.parent, pointer.point.position.x, pointer.point.position.y);
        breakdownPopup.x = Math.max(8, Math.min(position.x, breakdownPopup.parent.width - breakdownPopup.width - 8));
        const below = position.y + 22;
        breakdownPopup.y = below + breakdownPopup.height < breakdownPopup.parent.height - 8
            ? below : Math.max(8, position.y - breakdownPopup.height - 12);
        breakdownPopup.open();
    }
    onLinkHovered: link => {
        if (link.length) {
            dismissDelay.stop();
            revealDelay.restart();
        } else {
            revealDelay.stop();
            dismissDelay.restart();
        }
    }
    onLinkActivated: link => revealBreakdown(link)
    onScrollingChanged: if (scrolling) dismissBreakdown()
    onVisibleChanged: if (!visible) dismissBreakdown()
    onMarkdownChanged: if (!markdown.length) dismissBreakdown()
    HoverHandler {
        id: pointer
        cursorShape: answer.hoveredLink.length ? Qt.PointingHandCursor : Qt.IBeamCursor
    }
    Timer {
        id: revealDelay
        interval: 220
        onTriggered: answer.revealBreakdown(answer.hoveredLink)
    }
    Timer {
        id: dismissDelay
        interval: 180
        onTriggered: if (!popoverHover.hovered && !answer.hoveredLink.length) answer.dismissBreakdown()
    }
    Popup {
        id: breakdownPopup
        objectName: "translationBreakdownPopup"
        parent: Overlay.overlay
        popupType: Popup.Item
        modal: false
        focus: false
        padding: Theme.space3
        width: Math.min(360, parent ? parent.width - 16 : 360)
        height: Math.min(breakdownLabel.implicitHeight + padding * 2, 280, parent ? parent.height - 16 : 280)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
            radius: Theme.controlRadius
        }
        contentItem: ScrollView {
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            HoverHandler {
                id: popoverHover
                onHoveredChanged: {
                    if (hovered) dismissDelay.stop();
                    else dismissDelay.restart();
                }
            }
            Label {
                id: breakdownLabel
                objectName: "translationBreakdownText"
                width: breakdownPopup.availableWidth
                text: answer.breakdownText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.ink
                font.pixelSize: Theme.body
                lineHeight: 1.3
                Accessible.name: text
            }
        }
    }
}
