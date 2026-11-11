/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "torrentmodel.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QVariantList>

struct SearchEngineInfo {
    QString id;
    QString name;
    QString url;
    QStringList categories;
    bool enabled = true;
    int resultCount = 0;
    bool running = false;
};

class TorrentSearchBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(TorrentFilterProxyModel *resultsModel READ resultsModel CONSTANT)
    Q_PROPERTY(int totalResultsCount READ totalResultsCount NOTIFY totalResultsCountChanged)
    Q_PROPERTY(int filteredResultsCount READ filteredResultsCount NOTIFY filteredResultsCountChanged)
    Q_PROPERTY(bool isSearching READ isSearching NOTIFY searchStateChanged)
    Q_PROPERTY(double searchProgress READ searchProgress NOTIFY searchStateChanged)
    Q_PROPERTY(int activeEnginesCount READ activeEnginesCount NOTIFY searchStateChanged)
    Q_PROPERTY(int completedEnginesCount READ completedEnginesCount NOTIFY searchStateChanged)
    Q_PROPERTY(int totalSearchEnginesCount READ totalSearchEnginesCount NOTIFY searchStateChanged)
    Q_PROPERTY(QString currentCategory READ currentCategory WRITE setCurrentCategory NOTIFY currentCategoryChanged)
    Q_PROPERTY(QString lastSearchQuery READ lastSearchQuery NOTIFY searchStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QVariantList engines READ engines NOTIFY enginesChanged)
    Q_PROPERTY(int enabledEnginesCount READ enabledEnginesCount NOTIFY enginesChanged)
    Q_PROPERTY(QStringList recentSearches READ recentSearches NOTIFY recentSearchesChanged)
    Q_PROPERTY(bool transmissionInstalled READ transmissionInstalled CONSTANT)
    Q_PROPERTY(QString novaRuntimePath READ novaRuntimePath CONSTANT)
    Q_PROPERTY(bool updatingEngines READ updatingEngines NOTIFY updatingEnginesChanged)

public:
    explicit TorrentSearchBackend(QObject *parent = nullptr);
    ~TorrentSearchBackend() override;

    bool updatingEngines() const { return m_updatingEngines; }

    TorrentFilterProxyModel *resultsModel() { return &m_proxyModel; }
    TorrentListModel *sourceModel() { return &m_sourceModel; }

    int totalResultsCount() const { return m_sourceModel.rowCount(); }
    int filteredResultsCount() const { return m_proxyModel.rowCount(); }
    bool isSearching() const { return m_activeProcesses.size() > 0; }
    double searchProgress() const;
    int activeEnginesCount() const { return m_activeProcesses.size(); }
    int completedEnginesCount() const { return m_completedEngines; }
    int totalSearchEnginesCount() const { return m_totalEnginesInSearch; }

    QString currentCategory() const { return m_currentCategory; }
    void setCurrentCategory(const QString &category);

    QString lastSearchQuery() const { return m_lastSearchQuery; }
    QString statusMessage() const { return m_statusMessage; }

    QVariantList engines() const;
    int enabledEnginesCount() const;
    QStringList recentSearches() const { return m_recentSearches; }
    bool transmissionInstalled() const { return m_transmissionInstalled; }
    QString novaRuntimePath() const { return m_novaDir; }

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void stopSearch();
    Q_INVOKABLE void clearResults();

    Q_INVOKABLE void downloadTorrent(int proxyRow);
    Q_INVOKABLE void downloadByLink(const QString &link, const QString &engineId, const QString &title = QString());
    Q_INVOKABLE void copyMagnetOrLink(const QString &link);
    Q_INVOKABLE void openDescriptionPage(const QString &descLink);
    Q_INVOKABLE void openTransmission();

    Q_INVOKABLE void setEngineEnabled(const QString &engineId, bool enabled);
    Q_INVOKABLE void setAllEnginesEnabled(bool enabled);
    Q_INVOKABLE bool installPluginFromFile(const QString &fileUrlOrPath);
    Q_INVOKABLE void refreshEngines();
    Q_INVOKABLE void updateEngines();
    Q_INVOKABLE void clearRecentSearches();

    static QVector<SearchEngineInfo> parseCapabilitiesXml(const QString &xmlContent);
    static QString pluginVersion(const QString &filePath);
    static QString versionFromHeader(const QByteArray &head);
    static int compareVersions(const QString &a, const QString &b);
    static QUrl upstreamPluginUrl(const QString &engineId);

Q_SIGNALS:
    void updatingEnginesChanged();
    void totalResultsCountChanged();
    void filteredResultsCountChanged();
    void searchStateChanged();
    void currentCategoryChanged();
    void statusMessageChanged();
    void enginesChanged();
    void recentSearchesChanged();
    void toastNotification(const QString &message, bool isError);

private:
    void ensureNovaRuntime();
    void loadEnginesCapabilities();
    void setStatusMessage(const QString &msg);
    void launchInTransmission(const QString &targetUriOrPath, const QString &displayName);
    void addRecentSearch(const QString &query);

    TorrentListModel m_sourceModel;
    TorrentFilterProxyModel m_proxyModel;
    QSettings m_settings;

    QString m_novaDir;
    QString m_currentCategory = QStringLiteral("all");
    QString m_lastSearchQuery;
    QString m_statusMessage;
    QStringList m_recentSearches;
    bool m_transmissionInstalled = false;

    QVector<SearchEngineInfo> m_engines;
    QList<QPointer<QProcess>> m_activeProcesses;
    QHash<QProcess *, QByteArray> m_processBuffers;
    int m_completedEngines = 0;
    int m_totalEnginesInSearch = 0;

    QNetworkAccessManager m_network;
    bool m_updatingEngines = false;
    int m_updatePending = 0;
    int m_updateFailures = 0;
    QStringList m_updatedEngines;
};
