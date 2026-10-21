#include "flashcard_deck.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QDateTime>
#include <stdexcept>
#include <cmath>
#include <QFileInfo>
namespace {
QString tsvCell(QString value) {
    return value.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
}
} // namespace
FlashcardDeck::FlashcardDeck(ProfileStore &profile, QObject *parent)
    : QObject(parent), store(profile), savedCards(profile.snapshot().value("cards").toArray()) {}
bool FlashcardDeck::persist() {
    if (!store.update({{"cards", savedCards}})) {
        setMessage(store.error());
        return false;
    }
    emit changed();
    return true;
}

FlashcardSaveResult FlashcardDeck::saveCard(const QString &front, const QString &context, const QString &back,
                             double seconds, const QString &mediaPath) {
    if (front.trimmed().isEmpty() || front.size() > 4000 || context.size() > 12000 ||
        savedCards.size() >= 10000 || !std::isfinite(seconds) || seconds < 0 ||
        back.toUtf8().size() > 64 * 1024) {
        setMessage("Choose a short selection; the deck holds at most 10,000 cards.");
        return {false, statusText};
    }

    if (back.trimmed().isEmpty()) {
        setMessage("Complete an explanation or load a translation before saving a card.");
        return {false, statusText};
    }
    const QJsonObject card{{"front", front},
                           {"back", back},
                           {"context", context},
                           {"media", QFileInfo(mediaPath).fileName()},
                           {"seconds", seconds},
                           {"reviews", 0},
                           {"due", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
    const auto previous = savedCards;
    savedCards.append(card);
    if (!persist()) {
        savedCards = previous;
        return {false, statusText};
    }
    setMessage("Flashcard saved on this device.");
    return {true, statusText};
}

void FlashcardDeck::removeCard(int index) {
    if (index < 0 || index >= savedCards.size())
        return;
    const auto previous = savedCards;
    savedCards.removeAt(index);
    if (!persist())
        savedCards = previous;
    emit changed();
}

void FlashcardDeck::reviewCard(int index, bool remembered) {
    if (index < 0 || index >= savedCards.size())
        return;
    const auto previous = savedCards;
    auto card = savedCards[index].toObject();
    const int reviews = remembered ? qMin(card.value("reviews").toInt() + 1, 10) : 0;
    card["reviews"] = reviews;
    card["due"] = QDateTime::currentDateTimeUtc()
                      .addSecs(remembered ? (1 << reviews) * 86400 : 600)
                      .toString(Qt::ISODate);
    savedCards[index] = card;
    if (!persist())
        savedCards = previous;
    emit changed();
}

void FlashcardDeck::exportCards(const QUrl &url) {
    if (!url.isLocalFile())
        return;
    QSaveFile file(url.toLocalFile());
    QByteArray data = "#separator:tab\n#html:false\n#columns:Front\tBack\tContext\tMedia\tSeconds\n";
    for (const auto &value : savedCards) {
        const auto card = value.toObject();
        QStringList cells;
        for (const auto &key : {"front", "back", "context", "media"})
            cells << tsvCell(card.value(key).toString());
        cells << QString::number(card.value("seconds").toDouble());
        data += (cells.join('\t') + '\n').toUtf8();
    }
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        setMessage("Could not export the deck.");
        return;
    }
    setMessage("Exported UTF-8 flashcards for Anki text import with HTML disabled.");
}
