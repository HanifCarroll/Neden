import QtQuick.Controls

Action {
    id: action
    // Qt's native menu listens to textChanged but not shortcutChanged.
    // Re-emit the unchanged label's notification to synchronize the entire
    // native item, including its key equivalent, after a shortcut is edited.
    // Keep this compatibility workaround here instead of in every command.
    // https://github.com/qt/qtdeclarative/blob/6.10/src/quicktemplates/qquicknativemenuitem.cpp
    // The signal carries QKeySequence, which qmllint cannot expose to QML.
    // This handler deliberately takes no arguments and never reads that value.
    // qmllint disable signal-handler-parameters
    onShortcutChanged: function() {
        action.textChanged(action.text);
    }
    // qmllint enable signal-handler-parameters
}
