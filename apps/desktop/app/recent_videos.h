#pragma once
#include <QAbstractListModel>
#include <QUrl>
#include <optional>
#include <QtQml/qqmlregistration.h>

struct RecentVideo {
    QString path;
    double position = 0;
    double duration = 0;
    QString primarySubtitlePath{};
    QString secondarySubtitlePath{};
};

class RecentVideos : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Recent videos are supplied by the application")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    enum Role {
        MediaTitle = Qt::UserRole + 1,
        MediaUrl,
        FilePath,
        ResumePosition,
        DurationSeconds,
        FileAvailable
    };
    explicit RecentVideos(QString directory, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return videos.size(); }
    QString error() const { return errorText; }
    std::optional<RecentVideo> find(const QString &path) const;
    bool remember(const RecentVideo &video, bool promote = false);
    Q_INVOKABLE void remove(const QUrl &url);
signals:
    void countChanged();
    void errorChanged();
    void removed(QString path);

private:
    void load();
    bool persist(const QList<RecentVideo> &next);
    void setError(const QString &message);
    QString storageFile;
    QList<RecentVideo> videos;
    QString errorText;
    bool validStorage = true;
};
