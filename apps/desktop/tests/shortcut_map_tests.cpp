#include "shortcut_map.h"
#include <QCoreApplication>
#include <QFile>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QtTest>

class ShortcutMapTests : public QObject {
    Q_OBJECT
private slots:
    void savesOverridesAndRestoresDefaults() {
        QTemporaryDir directory;
        ShortcutMap map;
        map.load(directory.path());
        QCOMPARE(map.bindings().value("toggleSidebar").toString(), QString("Ctrl+B"));
        QVERIFY(map.assign("toggleSidebar", "Ctrl+Shift+B"));
        QVERIFY(map.assign("toggleMute", ""));
        ShortcutMap restored;
        restored.load(directory.path());
        QCOMPARE(restored.bindings(), map.bindings());
        QVERIFY(restored.reset("toggleSidebar"));
        QCOMPARE(restored.bindings().value("toggleSidebar").toString(), QString("Ctrl+B"));
        QVERIFY(restored.resetAll());
        QCOMPARE(restored.bindings().value("toggleMute").toString(), QString("M"));
    }
    void rejectsConflictsAndReservedKeysWithoutChangingBindings() {
        ShortcutMap map;
        const auto original = map.bindings();
        QVERIFY(!map.assign("toggleSidebar", "Space"));
        QVERIFY(map.message().contains("Play / pause"));
        for (const auto &binding : {"Ctrl+Q", "Ctrl+C", "Escape", "Tab", "Ctrl+K, Ctrl+P"})
            QVERIFY(!map.assign("toggleSidebar", binding));
        QVERIFY(!map.assign("unknown", "B"));
        QCOMPARE(map.bindings(), original);
        QVERIFY(map.assign("toggleMute", ""));
        QVERIFY(map.assign("toggleSidebar", "M"));
        QVERIFY(!map.reset("toggleMute"));
    }
    void recordsWithoutTriggeringActionsAndEscapeCancels() {
        ShortcutMap map;
        QObject receiver;
        map.beginRecording("toggleSidebar");
        QKeyEvent overrideEvent(QEvent::ShortcutOverride, Qt::Key_B, Qt::ControlModifier | Qt::ShiftModifier);
        QCoreApplication::sendEvent(&receiver, &overrideEvent);
        QVERIFY(overrideEvent.isAccepted());
        QKeyEvent event(QEvent::KeyPress, Qt::Key_B, Qt::ControlModifier | Qt::ShiftModifier);
        QCoreApplication::sendEvent(&receiver, &event);
        QVERIFY(map.saveRecording());
        QCOMPARE(map.bindings().value("toggleSidebar").toString(), QString("Ctrl+Shift+B"));
        QVERIFY(!map.recording());
        map.beginRecording("toggleMute");
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(&receiver, &escape);
        QVERIFY(!map.recording());
        QCOMPARE(map.bindings().value("toggleMute").toString(), QString("M"));
    }
    void corruptOrConflictingFilesUseDefaults() {
        QTemporaryDir directory;
        QFile file(directory.filePath("shortcuts.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"version\":1,\"bindings\":{\"toggleSidebar\":\"M\"}}"); file.close();
        ShortcutMap map;
        map.load(directory.path());
        QCOMPARE(map.bindings().value("toggleSidebar").toString(), QString("Ctrl+B"));
        QVERIFY(map.message().contains("conflict"));
    }
};
QTEST_GUILESS_MAIN(ShortcutMapTests)
#include "shortcut_map_tests.moc"
