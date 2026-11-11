/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVector>
#include <optional>

struct TorrentItem {
    QString link;
    QString name;
    qint64 sizeBytes = -1;
    QString formattedSize;
    int seeds = 0;
    int leechers = 0;
    QString engineId;
    QString engineName;
    QString engineUrl;
    QString descLink;
    qint64 pubDate = -1;
    QString formattedDate;
    bool isMagnet = false;
    QString dedupKey;
};

class TorrentListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        LinkRole = Qt::UserRole + 1,
        NameRole,
        SizeBytesRole,
        FormattedSizeRole,
        SeedsRole,
        LeechersRole,
        EngineIdRole,
        EngineNameRole,
        EngineUrlRole,
        DescLinkRole,
        PubDateRole,
        FormattedDateRole,
        IsMagnetRole
    };
    Q_ENUM(Roles)

    explicit TorrentListModel(QObject *parent = nullptr);
    ~TorrentListModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    bool addOrMergeItem(const TorrentItem &item);
    const TorrentItem *itemAt(int row) const;

    static std::optional<TorrentItem> parseNovaLine(const QString &line,
                                                    const QString &engineId = QString(),
                                                    const QString &engineName = QString());
    static QString formatSize(qint64 bytes);
    static QString formatPubDate(qint64 unixTimestamp);
    static QString extractDedupKey(const QString &link, const QString &name);

Q_SIGNALS:
    void countChanged();

private:
    QVector<TorrentItem> m_items;
    QHash<QString, int> m_dedupIndex;
};

class TorrentFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString engineFilter READ engineFilter WRITE setEngineFilter NOTIFY engineFilterChanged)
    Q_PROPERTY(int minSeeds READ minSeeds WRITE setMinSeeds NOTIFY minSeedsChanged)
    Q_PROPERTY(QString sortField READ sortField WRITE setSortField NOTIFY sortFieldChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortDescendingChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit TorrentFilterProxyModel(QObject *parent = nullptr);
    ~TorrentFilterProxyModel() override = default;

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString &text);

    QString engineFilter() const { return m_engineFilter; }
    void setEngineFilter(const QString &engineId);

    int minSeeds() const { return m_minSeeds; }
    void setMinSeeds(int minSeeds);

    QString sortField() const { return m_sortField; }
    void setSortField(const QString &field);

    bool sortDescending() const { return m_sortDescending; }
    void setSortDescending(bool descending);

    Q_INVOKABLE QVariantMap getRow(int proxyRow) const;

Q_SIGNALS:
    void filterTextChanged();
    void engineFilterChanged();
    void minSeedsChanged();
    void sortFieldChanged();
    void sortDescendingChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;

private:
    void applySorting();

    QString m_filterText;
    QStringList m_filterTokens;
    QString m_engineFilter = QStringLiteral("all");
    int m_minSeeds = 0;
    QString m_sortField = QStringLiteral("seeds");
    bool m_sortDescending = true;
};
