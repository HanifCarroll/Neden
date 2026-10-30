#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QtQml/qqmlregistration.h>

// One registry supplies both the editor and live bindings. Only overrides are saved.
class ShortcutMap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList commands READ commands NOTIFY changed)
    Q_PROPERTY(QVariantMap bindings READ bindings NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(QString recordingLabel READ recordingLabel NOTIFY recordingChanged)
    Q_PROPERTY(QString candidateLabel READ candidateLabel NOTIFY candidateChanged)
public:
    explicit ShortcutMap(QObject *parent = nullptr);
    ~ShortcutMap() override;
    void load(const QString &directory);
    QVariantList commands() const;
    QVariantMap bindings() const;
    QString message() const { return status; }
    bool recording() const { return !recordingId.isEmpty(); }
    QString recordingLabel() const;
    QString candidateLabel() const;
    Q_INVOKABLE bool matches(const QString &id, int key, int modifiers) const;
    Q_INVOKABLE bool assign(const QString &id, const QString &sequence);
    Q_INVOKABLE bool reset(const QString &id);
    Q_INVOKABLE bool resetAll();
    Q_INVOKABLE void beginRecording(const QString &id);
    Q_INVOKABLE void cancelRecording();
    Q_INVOKABLE bool saveRecording();
    Q_INVOKABLE QString conflict(const QString &id, const QString &sequence) const;
signals:
    void changed();
    void messageChanged();
    void recordingChanged();
    void candidateChanged();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    bool persist(const QMap<QString, QString> &values);
    void setMessage(const QString &message);
    QMap<QString, QString> overrides;
    QString storageFile;
    QString status;
    QString recordingId;
    QString candidate;
};
