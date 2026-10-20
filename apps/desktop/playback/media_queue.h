#pragma once
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Owns ordering and progression, independently of decoding and the current study session.
class MediaQueue : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Queue belongs to the player")
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY changed)
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY changed)
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY changed)
public:
    using QObject::QObject;
    QVariantList entries() const;
    int currentIndex() const { return selectedIndex; }
    bool hasNext() const { return selectedIndex + 1 < files.size(); }
    bool hasPrevious() const { return selectedIndex > 0; }
    bool autoplay() const { return advanceAutomatically; }
    void setAutoplay(bool value);
    void replace(const QList<QUrl> &urls);
    void setCurrent(const QUrl &url);
    Q_INVOKABLE void append(const QList<QUrl> &urls);
    Q_INVOKABLE void play(int index);
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void clear();
    static QList<QUrl> expand(const QList<QUrl> &urls);
signals:
    void changed();
    void openRequested(QUrl url);
private:
    QList<QUrl> files;
    int selectedIndex = -1;
    bool advanceAutomatically = false;
};
