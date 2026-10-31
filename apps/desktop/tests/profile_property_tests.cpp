#include "app_settings.h"
#include "flashcard_deck.h"
#include "property_support.h"
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <memory>

class ProfilePropertyTests : public QObject {
    Q_OBJECT
    static QByteArray bytes(const QString &directory) {
        QFile file(directory + "/study.json");
        if (!file.open(QIODevice::ReadOnly))
            qFatal("Cannot read synthetic profile");
        return file.readAll();
    }
private slots:
    void settingsAndCardsRemainIndependentAcrossSequences_data() { PropertySupport::seeds(32); }
    void settingsAndCardsRemainIndependentAcrossSequences() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto profile = std::make_unique<ProfileStore>(directory.path());
        auto settings = std::make_unique<AppSettings>(*profile);
        auto deck = std::make_unique<FlashcardDeck>(*profile);
        QVERIFY(settings->configure("http://127.0.0.1:12345", "initial", "https://example.org/%s", false));
        QString expectedModel = "initial";
        QStringList expectedFronts;
        for (int step = 0; step < 48; ++step) {
            const auto snapshot = profile->snapshot();
            const auto saved = bytes(directory.path());
            switch (PropertySupport::number(random, 0, 5)) {
            case 0:
                expectedModel = QString("model-%1").arg(step);
                QVERIFY(settings->configure("http://127.0.0.1:12345", expectedModel, "https://example.org/%s",
                                            false));
                QCOMPARE(profile->snapshot().value("cards"), snapshot.value("cards"));
                break;
            case 1: {
                const auto front = PropertySupport::text(random);
                QVERIFY(deck->saveCard(front, "context", "answer", step, "/synthetic/video.mkv").success);
                expectedFronts.append(front);
                auto before = snapshot, after = profile->snapshot();
                before.remove("cards");
                after.remove("cards");
                QCOMPARE(after, before);
                break;
            }
            case 2:
                if (!expectedFronts.isEmpty()) {
                    const int index = PropertySupport::number(random, 0, expectedFronts.size() - 1);
                    deck->removeCard(index);
                    expectedFronts.removeAt(index);
                }
                break;
            case 3:
                QVERIFY(!settings->configure("file:///invalid", "invalid", "https://example.org/%s", false));
                QVERIFY(!deck->saveCard("", "context", "answer", step).success);
                QCOMPARE(profile->snapshot(), snapshot);
                QCOMPARE(bytes(directory.path()), saved);
                break;
            case 4:
                if (!expectedFronts.isEmpty()) {
                    const int index = PropertySupport::number(random, 0, expectedFronts.size() - 1);
                    const auto previous = deck->cards()[index].toMap();
                    const bool remembered = PropertySupport::number(random, 0, 1);
                    deck->reviewCard(index, remembered);
                    const auto next = deck->cards()[index].toMap();
                    QCOMPARE(next.value("reviews").toInt(),
                             remembered ? qMin(previous.value("reviews").toInt() + 1, 10) : 0);
                    QCOMPARE(next.value("back"), previous.value("back"));
                    QCOMPARE(next.value("context"), previous.value("context"));
                }
                break;
            case 5:
                deck.reset();
                settings.reset();
                profile = std::make_unique<ProfileStore>(directory.path());
                settings = std::make_unique<AppSettings>(*profile);
                deck = std::make_unique<FlashcardDeck>(*profile);
                QCOMPARE(profile->snapshot(), snapshot);
                break;
            }
            QVERIFY(profile->readable());
            QCOMPARE(settings->model(), expectedModel);
            QStringList actualFronts;
            for (const auto &card : deck->cards())
                actualFronts.append(card.toMap().value("front").toString());
            QCOMPARE(actualFronts, expectedFronts);
            ProfileStore reloaded(directory.path());
            QVERIFY(reloaded.readable());
            QCOMPARE(reloaded.snapshot(), profile->snapshot());
        }
    }

    void failedWritesPreserveMemoryAndDisk() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        FlashcardDeck deck(profile);
        QVERIFY(settings.configure("http://127.0.0.1:12345", "saved", "https://example.org/%s", false));
        QVERIFY(deck.saveCard("front", "context", "back", 1).success);
        const auto saved = bytes(directory.path());
        const auto snapshot = profile.snapshot();
        QVERIFY(!profile.update({{"oversized", QString(8 * 1024 * 1024, 'x')}}));
        QCOMPARE(profile.snapshot(), snapshot);
        QCOMPARE(bytes(directory.path()), saved);

        // A directory at the output path forces QSaveFile to fail on every platform.
        const auto path = directory.filePath("study.json");
        const auto backup = directory.filePath("preserved.json");
        QVERIFY(QFile::rename(path, backup));
        QVERIFY(QDir().mkdir(path));
        QVERIFY(!settings.configure("http://127.0.0.1:12345", "unsaved", "https://example.org/%s", false));
        QVERIFY(!deck.saveCard("unsaved", "context", "back", 2).success);
        deck.removeCard(0);
        deck.reviewCard(0, true);
        QCOMPARE(settings.model(), QString("saved"));
        QCOMPARE(deck.cards(), snapshot.value("cards").toArray().toVariantList());
        QCOMPARE(profile.snapshot(), snapshot);
        QVERIFY(QDir().rmdir(path));
        QVERIFY(QFile::rename(backup, path));
        QCOMPARE(bytes(directory.path()), saved);
    }

    void corruptDocumentsRemainUntouched_data() { PropertySupport::seeds(); }
    void corruptDocumentsRemainUntouched() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        const QList<QByteArray> documents{"{broken", "[]", "{\"version\":2,\"cards\":[]}",
                                          "{\"version\":1,\"cards\":[{}]}"};
        const auto data = documents[PropertySupport::number(random, 0, documents.size() - 1)] +
                          QByteArray(PropertySupport::number(random, 0, 50), ' ');
        QTemporaryDir directory;
        QFile file(directory.filePath("study.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
        file.close();
        ProfileStore profile(directory.path());
        QVERIFY(!profile.readable());
        QVERIFY(!profile.update({{"cards", QJsonArray{}}}));
        QCOMPARE(bytes(directory.path()), data);
    }
};
QTEST_GUILESS_MAIN(ProfilePropertyTests)
#include "profile_property_tests.moc"
