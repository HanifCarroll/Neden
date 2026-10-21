#pragma once
#include <QAbstractListModel>
#include <QUrl>
#include <QSortFilterProxyModel>
#include <optional>
#include <QtQml/qqmlregistration.h>

struct RecentVideo {
    QString path;
    double position = 0;
    double duration = 0;
    QString primarySubtitlePath{};
    QString secondarySubtitlePath{};
    double primaryDelay = 0, secondaryDelay = 0;
    QString primaryNativeTrack{}, secondaryNativeTrack{};
    bool finished = false;
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
        FileAvailable,
        Finished
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
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setFinished(const QUrl &url, bool finished);
    Q_INVOKABLE bool relocate(const QUrl &oldUrl, const QUrl &newUrl);
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

// A view of history that can hide completed entries without discarding their settings.
class RecentVideosFilter : public QSortFilterProxyModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(RecentVideos *history READ history WRITE setHistory NOTIFY historyChanged)
    Q_PROPERTY(bool includeFinished READ includeFinished WRITE setIncludeFinished NOTIFY includeFinishedChanged)
public:
    explicit RecentVideosFilter(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
        setFilterRole(RecentVideos::Finished);
    }
    RecentVideos *history() const { return qobject_cast<RecentVideos *>(sourceModel()); }
    void setHistory(RecentVideos *history);
    bool includeFinished() const { return showFinished; }
    void setIncludeFinished(bool include);
signals:
    void historyChanged();
    void includeFinishedChanged();
protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
private:
    bool showFinished = false;
};
