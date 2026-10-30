#include "shortcut_map.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSaveFile>
#include <array>

namespace {
struct Command {
    const char *id;
    const char *name;
    const char *group;
    const char *shortcut;
};
constexpr std::array commands = {
    Command{"togglePlayback", "Play / pause", "Playback", "Space"},
    Command{"seekBackward", "Seek backward 5 seconds", "Playback", "Left"},
    Command{"seekForward", "Seek forward 5 seconds", "Playback", "Right"},
    Command{"toggleMute", "Mute / unmute", "Playback", "M"},
    Command{"volumeUp", "Volume up", "Playback", "Ctrl+Up"},
    Command{"volumeDown", "Volume down", "Playback", "Ctrl+Down"},
    Command{"jumpToTime", "Jump to time", "Playback", "Ctrl+J"},
    Command{"previousSubtitle", "Previous subtitle", "Subtitles", "Ctrl+Left"},
    Command{"nextSubtitle", "Next subtitle", "Subtitles", "Ctrl+Right"},
    Command{"explainSubtitle", "Explain current subtitle", "Study", "E"},
    Command{"replaySubtitle", "Replay subtitle", "Subtitles", "Ctrl+R"},
    Command{"primarySubtitles", "Load primary subtitles", "Subtitles", ""},
    Command{"secondarySubtitles", "Load secondary subtitles", "Subtitles", ""},
    Command{"openVideo", "Open video", "Files", "Ctrl+O"},
    Command{"openUrl", "Open URL", "Files", "Ctrl+Shift+O"},
    Command{"previousVideo", "Previous video", "Files", "Ctrl+Shift+Left"},
    Command{"nextVideo", "Next video", "Files", "Ctrl+Shift+Right"},
    Command{"recentVideos", "Recently played", "Files", "Ctrl+L"},
    Command{"toggleSidebar", "Show / hide sidebar", "View", "Ctrl+B"},
    Command{"instantFullscreen", "Instant fullscreen", "View", "F"},
    Command{"dictionary", "Dictionary", "Study", "Ctrl+D"}
};
const Command *findCommand(const QString &id) {
    for (const auto &command : commands)
        if (id == command.id)
            return &command;
    return nullptr;
}
QString normalized(const QString &sequence) {
    return QKeySequence::fromString(sequence, QKeySequence::PortableText).toString(QKeySequence::PortableText);
}
QString display(const QString &sequence) {
    return sequence.isEmpty() ? QString("Unassigned") : QKeySequence::fromString(sequence, QKeySequence::PortableText).toString(QKeySequence::NativeText);
}
QString invalidSequence(const QString &sequence) {
    if (sequence.isEmpty())
        return {};
    const auto keySequence = QKeySequence::fromString(sequence, QKeySequence::PortableText);
    if (keySequence.count() != 1 || keySequence[0].key() == Qt::Key_unknown || keySequence[0].key() == 0)
        return "Press one key combination.";
    const auto key = keySequence[0].key();
    if (key == Qt::Key_Escape || key == Qt::Key_Tab || key == Qt::Key_Backtab
        || key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_F11)
        return "This key is reserved for navigation or system controls.";
    const QStringList reserved{"Ctrl+Q", "Ctrl+W", "Ctrl+,", "Ctrl+C", "Ctrl+V", "Ctrl+X", "Ctrl+A", "Ctrl+Z", "Ctrl+Shift+Z", "Ctrl+Y", "Ctrl+Meta+F", "Ctrl+Space", "Meta+Space", "Alt+Tab"};
    for (const auto &binding : reserved)
        if (keySequence == QKeySequence::fromString(binding, QKeySequence::PortableText))
            return "This combination is reserved for standard editing or system controls.";
    return {};
}
}
ShortcutMap::ShortcutMap(QObject *parent) : QObject(parent) {}
ShortcutMap::~ShortcutMap() {
    if (qApp)
        qApp->removeEventFilter(this);
}
QVariantMap ShortcutMap::bindings() const {
    QVariantMap values;
    for (const auto &command : ::commands)
        values.insert(command.id, overrides.value(command.id, command.shortcut));
    return values;
}
bool ShortcutMap::matches(const QString &id, int key, int modifiers) const {
    const auto sequence = bindings().value(id).toString();
    if (sequence.isEmpty())
        return false;
    const auto keys = Qt::KeyboardModifiers(modifiers) &
                      (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    return QKeySequence(sequence, QKeySequence::PortableText) ==
           QKeySequence(QKeyCombination(keys, static_cast<Qt::Key>(key)));
}

QVariantList ShortcutMap::commands() const {
    QVariantList rows;
    for (const auto &command : ::commands) {
        const QString sequence = overrides.value(command.id, command.shortcut);
        rows.append(QVariantMap{{"id", command.id}, {"name", command.name}, {"group", command.group},
            {"sequence", sequence}, {"display", display(sequence)}, {"modified", overrides.contains(command.id)}});
    }
    return rows;
}
void ShortcutMap::setMessage(const QString &message) {
    status = message;
    emit messageChanged();
}
QString ShortcutMap::conflict(const QString &id, const QString &sequence) const {
    if (!findCommand(id))
        return "Unknown command.";
    if (const auto error = invalidSequence(sequence); !error.isEmpty())
        return error;
    if (sequence.isEmpty())
        return {};
    const auto values = bindings();
    for (const auto &command : ::commands)
        if (id != command.id && normalized(values.value(command.id).toString()) == normalized(sequence))
            return QString("Already assigned to %1. Clear or change that shortcut first.").arg(command.name);
    return {};
}
bool ShortcutMap::persist(const QMap<QString, QString> &values) {
    if (!storageFile.isEmpty()) {
        QJsonObject entries;
        for (auto entry = values.cbegin(); entry != values.cend(); ++entry)
            entries.insert(entry.key(), entry.value());
        QSaveFile file(storageFile);
        if (!QDir().mkpath(QFileInfo(storageFile).absolutePath()) || !file.open(QIODevice::WriteOnly)) {
            setMessage("Could not save shortcuts. Your previous bindings are unchanged.");
            return false;
        }
        const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"bindings", entries}}).toJson();
        if (file.write(data) != data.size() || !file.commit()) {
            setMessage("Could not save shortcuts. Your previous bindings are unchanged.");
            return false;
        }
    }
    overrides = values;
    emit changed();
    setMessage("Shortcuts saved.");
    return true;
}
void ShortcutMap::load(const QString &directory) {
    storageFile = QDir(directory).filePath("shortcuts.json");
    QFile file(storageFile);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) {
        setMessage("Could not read saved shortcuts. Defaults are active.");
        return;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || document.object().value("version").toInt() != 1
        || !document.object().value("bindings").isObject()) {
        setMessage("Saved shortcuts are invalid. Defaults are active.");
        return;
    }
    const auto entries = document.object().value("bindings").toObject();
    QMap<QString, QString> loaded;
    for (auto entry = entries.begin(); entry != entries.end(); ++entry) {
        if (!findCommand(entry.key()))
            continue;
        if (!entry.value().isString() || !invalidSequence(entry.value().toString()).isEmpty()) {
            setMessage("Saved shortcuts are invalid. Defaults are active.");
            return;
        }
        loaded.insert(entry.key(), normalized(entry.value().toString()));
    }
    overrides = loaded;
    for (const auto &command : ::commands) {
        if (!conflict(command.id, overrides.value(command.id, command.shortcut)).isEmpty()) {
            overrides.clear();
            setMessage("Saved shortcuts contain a conflict. Defaults are active.");
            emit changed();
            return;
        }
    }
    emit changed();
}
bool ShortcutMap::assign(const QString &id, const QString &sequence) {
    if (const auto error = conflict(id, sequence); !error.isEmpty()) {
        setMessage(error);
        return false;
    }
    auto values = overrides;
    const auto binding = normalized(sequence);
    if (binding == findCommand(id)->shortcut)
        values.remove(id);
    else
        values.insert(id, binding);
    return persist(values);
}
bool ShortcutMap::reset(const QString &id) {
    const auto *command = findCommand(id);
    return command && assign(id, command->shortcut);
}
bool ShortcutMap::resetAll() { return persist({}); }
QString ShortcutMap::recordingLabel() const {
    const auto *command = findCommand(recordingId);
    return command ? QString(command->name) : QString();
}
QString ShortcutMap::candidateLabel() const { return candidate.isEmpty() ? QString("Press a shortcut…") : display(candidate); }
void ShortcutMap::beginRecording(const QString &id) {
    if (!findCommand(id))
        return;
    recordingId = id;
    candidate.clear();
    setMessage("");
    qApp->installEventFilter(this);
    emit recordingChanged();
    emit candidateChanged();
}
void ShortcutMap::cancelRecording() {
    qApp->removeEventFilter(this);
    recordingId.clear();
    candidate.clear();
    emit recordingChanged();
    emit candidateChanged();
}
bool ShortcutMap::saveRecording() {
    if (candidate.isEmpty() || !assign(recordingId, candidate))
        return false;
    cancelRecording();
    return true;
}
bool ShortcutMap::eventFilter(QObject *object, QEvent *event) {
    Q_UNUSED(object);
    if (!recording())
        return false;
    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    if (event->type() != QEvent::KeyPress)
        return false;
    const auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Escape || keyEvent->key() == Qt::Key_Tab) {
        cancelRecording();
        return true;
    }
    if (keyEvent->isAutoRepeat() || keyEvent->key() == Qt::Key_Shift || keyEvent->key() == Qt::Key_Control
        || keyEvent->key() == Qt::Key_Meta || keyEvent->key() == Qt::Key_Alt || keyEvent->key() == Qt::Key_unknown)
        return true;
    const auto modifiers = keyEvent->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    candidate = QKeySequence(QKeyCombination(modifiers, static_cast<Qt::Key>(keyEvent->key()))).toString(QKeySequence::PortableText);
    setMessage(conflict(recordingId, candidate));
    emit candidateChanged();
    return true;
}
