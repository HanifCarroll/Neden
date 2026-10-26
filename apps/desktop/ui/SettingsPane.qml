pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Neden

ColumnLayout {
    id: pane
    required property SubtitleAppearance appearance
    property ShortcutMap keymap: ShortcutMap {}
    required property LearningSession session
    required property FullscreenController fullscreenController
    readonly property AppSettings settings: session.study.settings
    property string appearanceMessage: ""
    function restoreAppearanceControls() {
        primarySize.value = appearance.primarySize;
        secondarySize.value = appearance.secondarySize;
        bottomMargin.value = appearance.bottomMargin;
        outline.checked = appearance.outline;
        backgroundOption.checked = appearance.background;
        bold.checked = appearance.bold;
        textColor.text = appearance.textColor;
        secondaryColor.text = appearance.secondaryColor;
        trackGap.value = appearance.gap;
        reverseOrder.checked = appearance.reversed;
        selectable.checked = appearance.selectable;
        bottomPercent.value = appearance.bottomPercent;
    }
    function previewAppearance() {
        const values = pane.appearance.snapshot();
        values.primarySize = Math.round(primarySize.value);
        values.secondarySize = Math.round(secondarySize.value);
        values.bottomMargin = Math.round(bottomMargin.value);
        values.outline = outline.checked;
        values.background = backgroundOption.checked;
        values.bold = bold.checked;
        values.textColor = textColor.text;
        values.secondaryColor = secondaryColor.text;
        values.gap = Math.round(trackGap.value);
        values.reversed = reverseOrder.checked;
        values.selectable = selectable.checked;
        values.bottomPercent = bottomPercent.value;
        return pane.appearance.preview(values);
    }
    property alias page: categories.currentIndex
    property bool controlsReady: false
    Component.onCompleted: controlsReady = true
    spacing: Theme.space5
    SegmentBar {
        id: categories
        onCurrentIndexChanged: {
            if (pane.controlsReady && currentIndex !== 0) {
                pane.appearance.cancelPreview();
                pane.restoreAppearanceControls();
            }
        }
        Layout.fillWidth: true
        SegmentButton {
            width: categories.availableWidth / Math.max(1, categories.count)
            text: "Subtitles"
        }
        SegmentButton {
            width: categories.availableWidth / Math.max(1, categories.count)
            text: "Playback"
        }
        SegmentButton {
            width: categories.availableWidth / Math.max(1, categories.count)
            text: "AI & accounts"
        }
        SegmentButton {
            width: categories.availableWidth / Math.max(1, categories.count)
            text: "Shortcuts"
        }
    }
    ScrollView {
        visible: categories.currentIndex === 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.space4
            Label {
                text: "Subtitle appearance"
                color: Theme.ink
                font.pixelSize: Theme.title
            }
            Label {
                text: "Adjust both subtitle tracks. Apply to keep your changes."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                text: "Primary text · " + Math.round(primarySize.value) + " px"
                color: Theme.ink
            }
            ControlSlider {
                id: primarySize
                objectName: "primarySizeSlider"
                from: 16
                to: 64
                stepSize: 1
                onMoved: pane.previewAppearance()
                value: pane.appearance.primarySize
                Layout.fillWidth: true
                Accessible.name: "Primary subtitle size"
            }
            Label {
                text: "Secondary text · " + Math.round(secondarySize.value) + " px"
                color: Theme.ink
            }
            ControlSlider {
                id: secondarySize
                from: 12
                to: 56
                stepSize: 1
                onMoved: pane.previewAppearance()
                value: pane.appearance.secondarySize
                Layout.fillWidth: true
                Accessible.name: "Secondary subtitle size"
            }
            Label {
                text: "Bottom margin · " + Math.round(bottomMargin.value) + " px"
                color: Theme.ink
            }
            ControlSlider {
                id: bottomMargin
                from: 8
                to: 160
                stepSize: 1
                onMoved: pane.previewAppearance()
                value: pane.appearance.bottomMargin
                Layout.fillWidth: true
                Accessible.name: "Subtitle bottom margin"
            }
            CheckBox {
                id: outline
                text: "Dark outline"
                onToggled: pane.previewAppearance()
                checked: pane.appearance.outline
            }
            CheckBox {
                id: bold
                text: "Semibold text"
                onToggled: pane.previewAppearance()
                checked: pane.appearance.bold
            }
            CheckBox {
                id: backgroundOption
                text: "Compact dark background"
                onToggled: pane.previewAppearance()
                checked: pane.appearance.background
            }
            SettingsDisclosure {
                objectName: "advancedAppearanceSettings"
                title: "More appearance options"
                Layout.fillWidth: true
                Label {
                    text: "Primary text color (hex)"
                    color: Theme.ink
                }
                SearchField {
                    id: textColor
                    onTextEdited: pane.previewAppearance()
                    text: pane.appearance.textColor
                    Layout.fillWidth: true
                    Accessible.name: "Subtitle text color"
                    maximumLength: 7
                }
                Label {
                    text: "Secondary text color (hex)"
                    color: Theme.ink
                }
                SearchField {
                    id: secondaryColor
                    text: pane.appearance.secondaryColor
                    onTextEdited: pane.previewAppearance()
                    maximumLength: 7
                    Layout.fillWidth: true
                    Accessible.name: "Secondary subtitle color"
                }
                Label {
                    text: "Space between tracks · " + Math.round(trackGap.value) + " px"
                    color: Theme.ink
                }
                ControlSlider {
                    id: trackGap
                    from: 0
                    to: 80
                    stepSize: 1
                    value: pane.appearance.gap
                    onMoved: pane.previewAppearance()
                    Layout.fillWidth: true
                    Accessible.name: "Space between subtitle tracks"
                }
                Label {
                    text: "Additional bottom offset · " + Math.round(bottomPercent.value) + "% of picture"
                    color: Theme.ink
                }
                ControlSlider {
                    id: bottomPercent
                    from: 0
                    to: 40
                    stepSize: 1
                    value: pane.appearance.bottomPercent
                    onMoved: pane.previewAppearance()
                    Layout.fillWidth: true
                    Accessible.name: "Relative subtitle bottom offset"
                }
                CheckBox {
                    id: reverseOrder
                    text: "Display secondary subtitles above primary"
                    checked: pane.appearance.reversed
                    onToggled: pane.previewAppearance()
                }
                CheckBox {
                    id: selectable
                    text: "Selectable subtitles and explanation controls"
                    checked: pane.appearance.selectable
                    onToggled: pane.previewAppearance()
                }
            }
            Label {
                text: "Closing Settings discards unapplied appearance changes."
                color: Theme.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.space2
                ActionButton {
                    text: "Preview"
                    onClicked: pane.appearanceMessage = pane.previewAppearance() ? "Previewing. Apply to keep these changes." : "Check subtitle sizes and hex colors."
                }
                ActionButton {
                    text: "Apply appearance"
                    primary: true
                    onClicked: pane.appearanceMessage = pane.previewAppearance() && pane.appearance.applyPreview() ? "Appearance saved." : "Couldn’t save appearance."
                }
                ActionButton {
                    text: "Restore defaults"
                    onClicked: {
                        pane.appearance.reset();
                        pane.restoreAppearanceControls();
                        pane.appearanceMessage = "Default appearance restored.";
                    }
                }
            }
            Label {
                text: pane.appearanceMessage
                wrapMode: Text.WordWrap
                color: Theme.muted
                Layout.fillWidth: true
            }
        }
    }
    ScrollView {
        visible: categories.currentIndex === 1
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: parent.width
            spacing: Theme.space5
            SettingsGroup {
                title: "Fullscreen"
                visible: pane.fullscreenController.supportsNativeMode
                Layout.fillWidth: true
                description: "Saved automatically. Applies the next time you enter fullscreen."
                ChoiceBox {
                    Layout.fillWidth: true
                    model: ["Instant · current desktop", "Native · separate Space"]
                    currentIndex: pane.fullscreenController.nativeMode ? 1 : 0
                    Accessible.name: "Fullscreen mode"
                    onActivated: pane.fullscreenController.nativeMode = currentIndex === 1
                }
            }
            PlaybackPreferences {
                Layout.fillWidth: true
                session: pane.session
            }
        }
    }
    ScrollView {
        visible: categories.currentIndex === 2
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true
        ServicesSettings {
            width: parent.width
            settings: pane.settings
            account: pane.session.study.account
        }
    }
    ShortcutEditor {
        visible: categories.currentIndex === 3
        Layout.fillWidth: true
        Layout.fillHeight: true
        keymap: pane.keymap
    }
    Label {
        visible: categories.currentIndex === 2
        text: pane.settings.message
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        color: Theme.muted
    }
}
