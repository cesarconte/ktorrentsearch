/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>

struct SearchEngineInfo {
    QString id;
    QString name;
    QString url;
    QStringList categories;
    bool enabled = true;
    int resultCount = 0;
    bool running = false;
};

class TorrentEngineManager : public QObject
{
    Q_OBJECT

public:
    explicit TorrentEngineManager(QObject *parent = nullptr);
    ~TorrentEngineManager() override = default;

    static QProcessEnvironment pythonEnvironment();
    static void copyDirectoryFiles(const QString &srcDirPath, const QString &dstDirPath, bool overwrite = false);

    static QVector<SearchEngineInfo> parseCapabilitiesXml(const QString &xmlContent);
    static QString pluginVersion(const QString &filePath);
    static QString versionFromHeader(const QByteArray &head);
    static int compareVersions(const QString &a, const QString &b);
    static QUrl upstreamPluginUrl(const QString &engineId);

    void ensureNovaRuntime(const QString &novaDir);
    void loadEnginesCapabilities(const QString &novaDir, QSettings &settings);

    QVector<SearchEngineInfo> &enginesList() { return m_engines; }
    const QVector<SearchEngineInfo> &enginesList() const { return m_engines; }

    QVariantList enginesVariantList() const;
    int enabledEnginesCount() const;
    bool isUpdatingEngines() const { return m_updatingEngines; }

    void setEngineEnabled(const QString &engineId, bool enabled, QSettings &settings);
    void setAllEnginesEnabled(bool enabled, QSettings &settings);
    bool installPluginFromFile(const QString &fileUrlOrPath, const QString &novaDir);
    void updateEngines(const QString &novaDir, QSettings &settings);

Q_SIGNALS:
    void enginesChanged();
    void updatingEnginesChanged();
    void statusMessageRequested(const QString &msg);
    void toastRequested(const QString &msg, bool isError);

private:
    QVector<SearchEngineInfo> m_engines;
    QNetworkAccessManager m_network;
    bool m_updatingEngines = false;
    int m_updatePending = 0;
    int m_updateFailures = 0;
    QStringList m_updatedEngines;
};
