/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "torrentmodel.h"

#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <QUrl>

TorrentListModel::TorrentListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TorrentListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant TorrentListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const TorrentItem &item = m_items.at(index.row());
    switch (role) {
    case LinkRole:
        return item.link;
    case NameRole:
        return item.name;
    case SizeBytesRole:
        return item.sizeBytes;
    case FormattedSizeRole:
        return item.formattedSize;
    case SeedsRole:
        return item.seeds;
    case LeechersRole:
        return item.leechers;
    case EngineIdRole:
        return item.engineId;
    case EngineNameRole:
        return item.engineName;
    case EngineUrlRole:
        return item.engineUrl;
    case DescLinkRole:
        return item.descLink;
    case PubDateRole:
        return item.pubDate;
    case FormattedDateRole:
        return item.formattedDate;
    case IsMagnetRole:
        return item.isMagnet;
    default:
        return {};
    }
}

QHash<int, QByteArray> TorrentListModel::roleNames() const
{
    return {
        {LinkRole, "link"},
        {NameRole, "name"},
        {SizeBytesRole, "sizeBytes"},
        {FormattedSizeRole, "formattedSize"},
        {SeedsRole, "seeds"},
        {LeechersRole, "leechers"},
        {EngineIdRole, "engineId"},
        {EngineNameRole, "engineName"},
        {EngineUrlRole, "engineUrl"},
        {DescLinkRole, "descLink"},
        {PubDateRole, "pubDate"},
        {FormattedDateRole, "formattedDate"},
        {IsMagnetRole, "isMagnet"},
    };
}

void TorrentListModel::clear()
{
    if (m_items.isEmpty()) {
        return;
    }
    beginResetModel();
    m_items.clear();
    m_dedupIndex.clear();
    endResetModel();
    Q_EMIT countChanged();
}

bool TorrentListModel::addOrMergeItem(const TorrentItem &item)
{
    if (item.link.isEmpty() || item.name.isEmpty()) {
        return false;
    }

    const QString key = item.dedupKey.isEmpty() ? extractDedupKey(item.link, item.name) : item.dedupKey;
    auto it = m_dedupIndex.find(key);
    if (it != m_dedupIndex.end()) {
        const int row = it.value();
        if (row >= 0 && row < m_items.size()) {
            TorrentItem &existing = m_items[row];
            bool changed = false;

            if (item.seeds > existing.seeds) {
                existing.seeds = item.seeds;
                changed = true;
            }
            if (item.leechers > existing.leechers) {
                existing.leechers = item.leechers;
                changed = true;
            }
            if (existing.sizeBytes <= 0 && item.sizeBytes > 0) {
                existing.sizeBytes = item.sizeBytes;
                existing.formattedSize = item.formattedSize;
                changed = true;
            }
            if (existing.pubDate <= 0 && item.pubDate > 0) {
                existing.pubDate = item.pubDate;
                existing.formattedDate = item.formattedDate;
                changed = true;
            }
            if (existing.descLink.isEmpty() && !item.descLink.isEmpty()) {
                existing.descLink = item.descLink;
                changed = true;
            }
            if (!item.engineName.isEmpty() && !existing.engineName.contains(item.engineName, Qt::CaseInsensitive)) {
                existing.engineName += QStringLiteral(" · ") + item.engineName;
                changed = true;
            }

            if (changed) {
                const QModelIndex idx = index(row, 0);
                Q_EMIT dataChanged(idx, idx);
            }
        }
        return false;
    }

    const int newRow = m_items.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    TorrentItem copy = item;
    copy.dedupKey = key;
    m_items.push_back(copy);
    m_dedupIndex.insert(key, newRow);
    endInsertRows();
    Q_EMIT countChanged();
    return true;
}

const TorrentItem *TorrentListModel::itemAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return nullptr;
    }
    return &m_items.at(row);
}

std::optional<TorrentItem> TorrentListModel::parseNovaLine(const QString &line,
                                                           const QString &engineId,
                                                           const QString &engineName)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || !trimmed.contains(QLatin1Char('|'))) {
        return std::nullopt;
    }

    const QStringList parts = trimmed.split(QLatin1Char('|'));
    if (parts.size() < 6) {
        return std::nullopt;
    }

    const QString link = parts.at(0).trimmed();
    const QString rawName = parts.at(1).trimmed();
    if (link.isEmpty() || rawName.isEmpty()) {
        return std::nullopt;
    }

    // Decode URL-encoded name from novaprinter.py (urllib.parse.quote)
    QByteArray encodedBytes = rawName.toUtf8();
    encodedBytes.replace('+', ' ');
    QString decodedName = QUrl::fromPercentEncoding(encodedBytes).trimmed();
    if (decodedName.isEmpty()) {
        decodedName = rawName;
    }

    bool okSize = false;
    qint64 sizeBytes = parts.at(2).trimmed().toLongLong(&okSize);
    if (!okSize) {
        sizeBytes = -1;
    }

    bool okSeeds = false;
    int seeds = parts.at(3).trimmed().toInt(&okSeeds);
    if (!okSeeds || seeds < 0) {
        seeds = 0;
    }

    bool okLeech = false;
    int leechers = parts.at(4).trimmed().toInt(&okLeech);
    if (!okLeech || leechers < 0) {
        leechers = 0;
    }

    const QString engineUrl = parts.at(5).trimmed();
    const QString descLink = (parts.size() >= 7) ? parts.at(6).trimmed() : QString();

    qint64 pubDate = -1;
    if (parts.size() >= 8) {
        bool okDate = false;
        pubDate = parts.at(7).trimmed().toLongLong(&okDate);
        if (!okDate) {
            pubDate = -1;
        }
    }

    TorrentItem item;
    item.link = link;
    item.name = decodedName;
    item.sizeBytes = sizeBytes;
    item.formattedSize = formatSize(sizeBytes);
    item.seeds = seeds;
    item.leechers = leechers;
    item.engineId = engineId;
    item.engineName = engineName.isEmpty() ? engineId : engineName;
    item.engineUrl = engineUrl;
    item.descLink = descLink;
    item.pubDate = pubDate;
    item.formattedDate = formatPubDate(pubDate);
    item.isMagnet = link.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive);
    item.dedupKey = extractDedupKey(link, decodedName);

    return item;
}

QString TorrentListModel::formatSize(qint64 bytes)
{
    if (bytes <= 0) {
        return QStringLiteral("—");
    }

    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;
    constexpr double TB = GB * 1024.0;

    if (bytes >= static_cast<qint64>(TB)) {
        return QString::number(bytes / TB, 'f', 2) + QStringLiteral(" TB");
    }
    if (bytes >= static_cast<qint64>(GB)) {
        return QString::number(bytes / GB, 'f', 2) + QStringLiteral(" GB");
    }
    if (bytes >= static_cast<qint64>(MB)) {
        return QString::number(bytes / MB, 'f', 1) + QStringLiteral(" MB");
    }
    if (bytes >= static_cast<qint64>(KB)) {
        return QString::number(bytes / KB, 'f', 0) + QStringLiteral(" KB");
    }
    return QString::number(bytes) + QStringLiteral(" B");
}

QString TorrentListModel::formatPubDate(qint64 unixTimestamp)
{
    if (unixTimestamp <= 0) {
        return QStringLiteral("—");
    }
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(unixTimestamp);
    if (!dt.isValid()) {
        return QStringLiteral("—");
    }
    return QLocale::system().toString(dt.date(), QStringLiteral("dd MMM yyyy"));
}

QString TorrentListModel::extractDedupKey(const QString &link, const QString &name)
{
    static const QRegularExpression btihRegex(
        QStringLiteral("xt=urn:btih:([A-Fa-f0-9]{40}|[A-Za-z2-7]{32})"),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = btihRegex.match(link);
    if (match.hasMatch()) {
        return QStringLiteral("btih:") + match.captured(1).toLower();
    }
    if (!link.isEmpty()) {
        return QStringLiteral("link:") + link.trimmed().toLower();
    }
    return QStringLiteral("name:") + name.trimmed().toLower();
}

// =============================================================================
// TorrentFilterProxyModel
// =============================================================================

TorrentFilterProxyModel::TorrentFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    connect(this, &QAbstractItemModel::rowsInserted, this, &TorrentFilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &TorrentFilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &TorrentFilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &TorrentFilterProxyModel::countChanged);
    applySorting();
}

void TorrentFilterProxyModel::setFilterText(const QString &text)
{
    if (m_filterText == text) {
        return;
    }
    beginFilterChange();
    m_filterText = text;
    m_filterTokens = text.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterTextChanged();
    Q_EMIT countChanged();
}

void TorrentFilterProxyModel::setEngineFilter(const QString &engineId)
{
    const QString norm = engineId.trimmed().isEmpty() ? QStringLiteral("all") : engineId.trimmed();
    if (m_engineFilter == norm) {
        return;
    }
    beginFilterChange();
    m_engineFilter = norm;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT engineFilterChanged();
    Q_EMIT countChanged();
}

void TorrentFilterProxyModel::setMinSeeds(int minSeeds)
{
    const int clamped = qMax(0, minSeeds);
    if (m_minSeeds == clamped) {
        return;
    }
    beginFilterChange();
    m_minSeeds = clamped;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT minSeedsChanged();
    Q_EMIT countChanged();
}

void TorrentFilterProxyModel::setSortField(const QString &field)
{
    if (m_sortField == field) {
        return;
    }
    m_sortField = field;
    applySorting();
    Q_EMIT sortFieldChanged();
}

void TorrentFilterProxyModel::setSortDescending(bool descending)
{
    if (m_sortDescending == descending) {
        return;
    }
    m_sortDescending = descending;
    applySorting();
    Q_EMIT sortDescendingChanged();
}

QVariantMap TorrentFilterProxyModel::getRow(int proxyRow) const
{
    QVariantMap map;
    if (proxyRow < 0 || proxyRow >= rowCount()) {
        return map;
    }

    const QModelIndex idx = index(proxyRow, 0);
    map.insert(QStringLiteral("link"), data(idx, TorrentListModel::LinkRole));
    map.insert(QStringLiteral("name"), data(idx, TorrentListModel::NameRole));
    map.insert(QStringLiteral("sizeBytes"), data(idx, TorrentListModel::SizeBytesRole));
    map.insert(QStringLiteral("formattedSize"), data(idx, TorrentListModel::FormattedSizeRole));
    map.insert(QStringLiteral("seeds"), data(idx, TorrentListModel::SeedsRole));
    map.insert(QStringLiteral("leechers"), data(idx, TorrentListModel::LeechersRole));
    map.insert(QStringLiteral("engineId"), data(idx, TorrentListModel::EngineIdRole));
    map.insert(QStringLiteral("engineName"), data(idx, TorrentListModel::EngineNameRole));
    map.insert(QStringLiteral("engineUrl"), data(idx, TorrentListModel::EngineUrlRole));
    map.insert(QStringLiteral("descLink"), data(idx, TorrentListModel::DescLinkRole));
    map.insert(QStringLiteral("pubDate"), data(idx, TorrentListModel::PubDateRole));
    map.insert(QStringLiteral("formattedDate"), data(idx, TorrentListModel::FormattedDateRole));
    map.insert(QStringLiteral("isMagnet"), data(idx, TorrentListModel::IsMagnetRole));
    return map;
}

bool TorrentFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid()) {
        return false;
    }

    const int seeds = sourceModel()->data(idx, TorrentListModel::SeedsRole).toInt();
    if (seeds < m_minSeeds) {
        return false;
    }

    if (m_engineFilter != QStringLiteral("all")) {
        const QString engineId = sourceModel()->data(idx, TorrentListModel::EngineIdRole).toString();
        const QString engineName = sourceModel()->data(idx, TorrentListModel::EngineNameRole).toString();
        if (engineId.compare(m_engineFilter, Qt::CaseInsensitive) != 0
            && !engineName.contains(m_engineFilter, Qt::CaseInsensitive)) {
            return false;
        }
    }

    if (!m_filterTokens.isEmpty()) {
        const QString name = sourceModel()->data(idx, TorrentListModel::NameRole).toString();
        const QString engineName = sourceModel()->data(idx, TorrentListModel::EngineNameRole).toString();
        for (const QString &token : m_filterTokens) {
            if (!name.contains(token, Qt::CaseInsensitive)
                && !engineName.contains(token, Qt::CaseInsensitive)) {
                return false;
            }
        }
    }

    return true;
}

bool TorrentFilterProxyModel::lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const
{
    if (m_sortField == QStringLiteral("size")) {
        const qint64 lSize = sourceModel()->data(sourceLeft, TorrentListModel::SizeBytesRole).toLongLong();
        const qint64 rSize = sourceModel()->data(sourceRight, TorrentListModel::SizeBytesRole).toLongLong();
        if (lSize != rSize) {
            return lSize < rSize;
        }
    } else if (m_sortField == QStringLiteral("leechers")) {
        const int lLeech = sourceModel()->data(sourceLeft, TorrentListModel::LeechersRole).toInt();
        const int rLeech = sourceModel()->data(sourceRight, TorrentListModel::LeechersRole).toInt();
        if (lLeech != rLeech) {
            return lLeech < rLeech;
        }
    } else if (m_sortField == QStringLiteral("date")) {
        const qint64 lDate = sourceModel()->data(sourceLeft, TorrentListModel::PubDateRole).toLongLong();
        const qint64 rDate = sourceModel()->data(sourceRight, TorrentListModel::PubDateRole).toLongLong();
        if (lDate != rDate) {
            return lDate < rDate;
        }
    } else if (m_sortField == QStringLiteral("name")) {
        const QString lName = sourceModel()->data(sourceLeft, TorrentListModel::NameRole).toString();
        const QString rName = sourceModel()->data(sourceRight, TorrentListModel::NameRole).toString();
        const int cmp = QString::compare(lName, rName, Qt::CaseInsensitive);
        if (cmp != 0) {
            return cmp < 0;
        }
    }

    // Default / tie-breaker: seeds
    const int lSeeds = sourceModel()->data(sourceLeft, TorrentListModel::SeedsRole).toInt();
    const int rSeeds = sourceModel()->data(sourceRight, TorrentListModel::SeedsRole).toInt();
    if (lSeeds != rSeeds) {
        return lSeeds < rSeeds;
    }

    const QString lName = sourceModel()->data(sourceLeft, TorrentListModel::NameRole).toString();
    const QString rName = sourceModel()->data(sourceRight, TorrentListModel::NameRole).toString();
    return QString::compare(lName, rName, Qt::CaseInsensitive) < 0;
}

void TorrentFilterProxyModel::applySorting()
{
    invalidate();
    sort(0, m_sortDescending ? Qt::DescendingOrder : Qt::AscendingOrder);
}
