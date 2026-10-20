#pragma once
#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class SubtitleAppearance : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Subtitle appearance is supplied by the application")
    Q_PROPERTY(QString secondaryColor READ secondaryColor NOTIFY changed)
    Q_PROPERTY(int gap READ gap NOTIFY changed)
    Q_PROPERTY(bool reversed READ reversed NOTIFY changed)
    Q_PROPERTY(bool selectable READ selectable NOTIFY changed)
    Q_PROPERTY(double bottomPercent READ bottomPercent NOTIFY changed)
    Q_PROPERTY(int primarySize READ primarySize NOTIFY changed)
    Q_PROPERTY(int secondarySize READ secondarySize NOTIFY changed)
    Q_PROPERTY(int bottomMargin READ bottomMargin NOTIFY changed)
    Q_PROPERTY(bool outline READ outline NOTIFY changed)
    Q_PROPERTY(bool background READ background NOTIFY changed)
    Q_PROPERTY(bool bold READ bold NOTIFY changed)
    Q_PROPERTY(QString textColor READ textColor NOTIFY changed)
public:
    explicit SubtitleAppearance(const QString &directory, QObject *parent = nullptr);
    int primarySize() const { return sourceSize; }
    int secondarySize() const { return translationSize; }
    int bottomMargin() const { return margin; }
    bool outline() const { return outlined; }
    bool background() const { return boxed; }
    bool bold() const { return semibold; }
    QString textColor() const { return color; }
    Q_INVOKABLE bool configure(int primary, int secondary, int bottom, bool outline, bool background,
                               bool bold, const QString &textColor);
    QString secondaryColor() const { return translationColor; }
    int gap() const { return trackGap; }
    bool reversed() const { return reverseOrder; }
    bool selectable() const { return selectionEnabled; }
    double bottomPercent() const { return relativeBottom; }
    Q_INVOKABLE QVariantMap snapshot() const;
    Q_INVOKABLE bool preview(const QVariantMap &values);
    Q_INVOKABLE bool applyPreview();
    Q_INVOKABLE void cancelPreview();
    Q_INVOKABLE void reset();
signals:
    void changed();

private:
    QSettings settings;
    int sourceSize = 28;
    int translationSize = 24;
    int margin = 36;
    bool outlined = true;
    bool boxed = false;
    bool semibold = true;
    QString color = "#ffffff";
    QString translationColor = "#ffffff";
    int trackGap = 12;
    bool reverseOrder = false;
    bool selectionEnabled = true;
    double relativeBottom = 0;
    QVariantMap savedValues;
};
