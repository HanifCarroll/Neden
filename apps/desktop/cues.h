#pragma once
#include <QAbstractListModel>
#include <QVector>
#include <QtQml/qqmlregistration.h>

struct Cue {
    qint64 startMs;
    qint64 endMs;
    QString text;
    QString translation;
};
QString subtitlePlainText(const QString &input);
QVector<Cue> parseSrt(const QString &input);
int cueAt(const QVector<Cue> &cues, qint64 positionMs);

class CueModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Model is owned by the learning session")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(SearchLanguage searchLanguage READ searchLanguage WRITE setSearchLanguage NOTIFY searchLanguageChanged)
public:
    enum SearchLanguage { Unicode, Turkish };
    Q_ENUM(SearchLanguage)
    enum Roles { Text = Qt::UserRole + 1, Translation, Start, CueIndex };
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return visible.size(); }
    SearchLanguage searchLanguage() const { return language; }
    void setSearchLanguage(SearchLanguage value);
    void replace(QVector<Cue> next);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE int rowForCue(int cueIndex) const { return visible.indexOf(cueIndex); }
    void setTranslationCues(const QVector<Cue> &translations);
    QVector<Cue> cues;
signals:
    void countChanged();
    void searchLanguageChanged();
private:
    QString searchKey(const QString &text) const;
    void rebuildSearchKeys();
    SearchLanguage language = Unicode;
    QString currentQuery;
    QVector<QString> searchKeys;
    QVector<int> visible;
};
