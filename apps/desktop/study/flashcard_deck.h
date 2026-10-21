#pragma once
#include "profile_store.h"
#include <QObject>
#include <QVariantList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
struct FlashcardSaveResult {
    bool success;
    QString message;
};

class FlashcardDeck : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Deck is supplied by the application")
    Q_PROPERTY(QVariantList cards READ cards NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
public:
    explicit FlashcardDeck(ProfileStore &store, QObject *parent = nullptr);
    QVariantList cards() const { return savedCards.toVariantList(); }
    QString message() const { return statusText; }
    FlashcardSaveResult saveCard(const QString &front, const QString &context, const QString &back, double seconds,
                  const QString &media = {});
    Q_INVOKABLE void removeCard(int index);
    Q_INVOKABLE void reviewCard(int index, bool remembered);
    Q_INVOKABLE void exportCards(const QUrl &file);
signals:
    void changed();
    void messageChanged();

private:
    bool persist();
    void setMessage(const QString &message) {
        statusText = message;
        emit messageChanged();
    }
    ProfileStore &store;
    QJsonArray savedCards;
    QString statusText;
};
