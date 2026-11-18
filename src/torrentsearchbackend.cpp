/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "torrentsearchbackend.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

TorrentSearchBackend::TorrentSearchBackend(QObject *parent)
    : QObject(parent)
    , m_sourceModel(this)
    , m_proxyModel(this)
    , m_settings(QStringLiteral("Cesar"), QStringLiteral("ktorrentsearch"))
    , m_engineManager(this)
    , m_launcher(this)
{
    m_proxyModel.setSourceModel(&m_sourceModel);

    connect(&m_sourceModel, &TorrentListModel::countChanged,
            this, &TorrentSearchBackend::totalResultsCountChanged);
    connect(&m_proxyModel, &TorrentFilterProxyModel::countChanged,
            this, &TorrentSearchBackend::filteredResultsCountChanged);

    connect(&m_engineManager, &TorrentEngineManager::enginesChanged,
            this, &TorrentSearchBackend::enginesChanged);
    connect(&m_engineManager, &TorrentEngineManager::updatingEnginesChanged,
            this, &TorrentSearchBackend::updatingEnginesChanged);
    connect(&m_engineManager, &TorrentEngineManager::statusMessageRequested,
            this, &TorrentSearchBackend::setStatusMessage);
    connect(&m_engineManager, &TorrentEngineManager::toastRequested,
            this, &TorrentSearchBackend::toastNotification);

    connect(&m_launcher, &TorrentClientLauncher::statusMessageRequested,
            this, &TorrentSearchBackend::setStatusMessage);
    connect(&m_launcher, &TorrentClientLauncher::toastRequested,
            this, &TorrentSearchBackend::toastNotification);

    m_recentSearches = m_settings.value(QStringLiteral("recentSearches")).toStringList();

    m_novaDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/ktorrentsearch/nova3");

    m_engineManager.ensureNovaRuntime(m_novaDir);
    m_engineManager.loadEnginesCapabilities(m_novaDir, m_settings);

    setStatusMessage(QStringLiteral("Listo · %1 motores activos · Destino: Transmission Qt")
                         .arg(enabledEnginesCount()));
}

TorrentSearchBackend::~TorrentSearchBackend()
{
    stopSearch();
}

double TorrentSearchBackend::searchProgress() const
{
    if (m_totalEnginesInSearch <= 0) {
        return 0.0;
    }
    return qBound(0.0, static_cast<double>(m_completedEngines) / static_cast<double>(m_totalEnginesInSearch), 1.0);
}

void TorrentSearchBackend::setCurrentCategory(const QString &category)
{
    const QString norm = category.trimmed().isEmpty() ? QStringLiteral("all") : category.trimmed().toLower();
    if (m_currentCategory == norm) {
        return;
    }
    m_currentCategory = norm;
    Q_EMIT currentCategoryChanged();

    if (!m_lastSearchQuery.trimmed().isEmpty()) {
        search(m_lastSearchQuery);
    }
}

void TorrentSearchBackend::search(const QString &query)
{
    const QString cleanQuery = query.trimmed();
    if (cleanQuery.isEmpty()) {
        setStatusMessage(QStringLiteral("Introduce un término de búsqueda."));
        return;
    }

    stopSearch();
    m_sourceModel.clear();
    m_lastSearchQuery = cleanQuery;
    addRecentSearch(cleanQuery);

    const QString novaScript = m_novaDir + QStringLiteral("/nova2.py");
    if (!QFileInfo::exists(novaScript)) {
        setStatusMessage(QStringLiteral("Error: No se encontró el motor nova2.py en ") + m_novaDir);
        Q_EMIT toastNotification(m_statusMessage, true);
        return;
    }

    auto &engines = m_engineManager.enginesList();
    QVector<int> activeIndices;
    for (int i = 0; i < engines.size(); ++i) {
        engines[i].resultCount = 0;
        engines[i].running = false;
        if (!engines[i].enabled) {
            continue;
        }
        if (m_currentCategory != QStringLiteral("all")
            && !engines[i].categories.isEmpty()
            && !engines[i].categories.contains(m_currentCategory)) {
            continue;
        }
        activeIndices.push_back(i);
    }

    if (activeIndices.isEmpty()) {
        setStatusMessage(QStringLiteral("Ningún motor habilitado soporta la categoría «%1».").arg(m_currentCategory));
        Q_EMIT enginesChanged();
        Q_EMIT searchStateChanged();
        return;
    }

    m_completedEngines = 0;
    m_totalEnginesInSearch = activeIndices.size();

    for (int idx : activeIndices) {
        SearchEngineInfo &eng = engines[idx];
        eng.running = true;

        auto *proc = new QProcess(this);
        proc->setWorkingDirectory(m_novaDir);
        proc->setProcessEnvironment(TorrentEngineManager::pythonEnvironment());
        m_activeProcesses.push_back(proc);
        m_processBuffers.insert(proc, QByteArray());

        const QString engineId = eng.id;
        const QString engineName = eng.name;

        auto consumeBuffer = [this, proc, engineId, engineName](bool flushRemainder) {
            QByteArray &buf = m_processBuffers[proc];
            buf.append(proc->readAllStandardOutput());

            int newlineIdx = -1;
            while ((newlineIdx = buf.indexOf('\n')) != -1) {
                const QByteArray rawLine = buf.left(newlineIdx).trimmed();
                buf.remove(0, newlineIdx + 1);
                if (rawLine.isEmpty()) {
                    continue;
                }
                const QString lineStr = QString::fromUtf8(rawLine);
                if (auto parsed = TorrentListModel::parseNovaLine(lineStr, engineId, engineName)) {
                    if (m_sourceModel.addOrMergeItem(*parsed)) {
                        for (SearchEngineInfo &e : m_engineManager.enginesList()) {
                            if (e.id == engineId) {
                                e.resultCount++;
                                break;
                            }
                        }
                    }
                }
            }

            if (flushRemainder && !buf.trimmed().isEmpty()) {
                const QString lineStr = QString::fromUtf8(buf.trimmed());
                buf.clear();
                if (auto parsed = TorrentListModel::parseNovaLine(lineStr, engineId, engineName)) {
                    if (m_sourceModel.addOrMergeItem(*parsed)) {
                        for (SearchEngineInfo &e : m_engineManager.enginesList()) {
                            if (e.id == engineId) {
                                e.resultCount++;
                                break;
                            }
                        }
                    }
                }
            }
        };

        connect(proc, &QProcess::readyReadStandardOutput, this, [consumeBuffer]() {
            consumeBuffer(false);
        });

        connect(proc, &QProcess::finished, this, [this, proc, engineId, consumeBuffer](int, QProcess::ExitStatus) {
            consumeBuffer(true);
            m_processBuffers.remove(proc);
            m_activeProcesses.removeAll(proc);
            proc->deleteLater();

            for (SearchEngineInfo &e : m_engineManager.enginesList()) {
                if (e.id == engineId) {
                    e.running = false;
                    break;
                }
            }

            m_completedEngines++;
            if (m_activeProcesses.isEmpty()) {
                setStatusMessage(QStringLiteral("Búsqueda completada: %1 torrents encontrados en %2 motores")
                                     .arg(m_sourceModel.rowCount())
                                     .arg(m_totalEnginesInSearch));
            } else {
                setStatusMessage(QStringLiteral("Buscando «%1»… %2 resultados (%3/%4 motores)")
                                     .arg(m_lastSearchQuery)
                                     .arg(m_sourceModel.rowCount())
                                     .arg(m_completedEngines)
                                     .arg(m_totalEnginesInSearch));
            }

            Q_EMIT enginesChanged();
            Q_EMIT searchStateChanged();
        });

        // 15-second per-engine watchdog
        QTimer::singleShot(15000, proc, [proc]() {
            if (proc->state() != QProcess::NotRunning) {
                proc->kill();
            }
        });

        QStringList args = {
            QStringLiteral("-I"),
            novaScript,
            engineId,
            m_currentCategory,
        };
        args.append(cleanQuery.split(QLatin1Char(' '), Qt::SkipEmptyParts));
        proc->start(QStringLiteral("python3"), args);
    }

    setStatusMessage(QStringLiteral("Buscando «%1» en %2 motores simultáneos…")
                         .arg(cleanQuery)
                         .arg(m_totalEnginesInSearch));
    Q_EMIT enginesChanged();
    Q_EMIT searchStateChanged();
}

void TorrentSearchBackend::stopSearch()
{
    if (m_activeProcesses.isEmpty()) {
        return;
    }

    const auto procs = m_activeProcesses;
    m_activeProcesses.clear();
    m_processBuffers.clear();

    for (const QPointer<QProcess> &proc : procs) {
        if (proc) {
            proc->disconnect(this);
            if (proc->state() != QProcess::NotRunning) {
                proc->kill();
                proc->waitForFinished(200);
            }
            proc->deleteLater();
        }
    }

    for (SearchEngineInfo &e : m_engineManager.enginesList()) {
        e.running = false;
    }

    setStatusMessage(QStringLiteral("Búsqueda detenida · %1 resultados obtenidos").arg(m_sourceModel.rowCount()));
    Q_EMIT enginesChanged();
    Q_EMIT searchStateChanged();
}

void TorrentSearchBackend::clearResults()
{
    stopSearch();
    m_sourceModel.clear();
    m_lastSearchQuery.clear();
    for (SearchEngineInfo &e : m_engineManager.enginesList()) {
        e.resultCount = 0;
    }
    setStatusMessage(QStringLiteral("Resultados limpiados"));
    Q_EMIT enginesChanged();
    Q_EMIT searchStateChanged();
}

void TorrentSearchBackend::downloadTorrent(int proxyRow)
{
    m_launcher.downloadTorrent(proxyRow, &m_proxyModel, m_novaDir);
}

void TorrentSearchBackend::downloadByLink(const QString &link, const QString &engineId, const QString &title)
{
    m_launcher.downloadByLink(link, engineId, title, m_novaDir);
}

void TorrentSearchBackend::copyMagnetOrLink(const QString &link)
{
    m_launcher.copyMagnetOrLink(link);
}

void TorrentSearchBackend::openDescriptionPage(const QString &descLink)
{
    m_launcher.openDescriptionPage(descLink);
}

void TorrentSearchBackend::openTransmission()
{
    m_launcher.openTransmission();
}

void TorrentSearchBackend::setEngineEnabled(const QString &engineId, bool enabled)
{
    m_engineManager.setEngineEnabled(engineId, enabled, m_settings);
}

void TorrentSearchBackend::setAllEnginesEnabled(bool enabled)
{
    m_engineManager.setAllEnginesEnabled(enabled, m_settings);
}

bool TorrentSearchBackend::installPluginFromFile(const QString &fileUrlOrPath)
{
    const bool ok = m_engineManager.installPluginFromFile(fileUrlOrPath, m_novaDir);
    if (ok) {
        m_engineManager.loadEnginesCapabilities(m_novaDir, m_settings);
    }
    return ok;
}

void TorrentSearchBackend::updateEngines()
{
    m_engineManager.updateEngines(m_novaDir, m_settings);
}

void TorrentSearchBackend::refreshEngines()
{
    m_engineManager.ensureNovaRuntime(m_novaDir);
    m_engineManager.loadEnginesCapabilities(m_novaDir, m_settings);
    setStatusMessage(QStringLiteral("Motores actualizados · %1 activos").arg(enabledEnginesCount()));
}

void TorrentSearchBackend::clearRecentSearches()
{
    m_recentSearches.clear();
    m_settings.setValue(QStringLiteral("recentSearches"), m_recentSearches);
    m_settings.sync();
    Q_EMIT recentSearchesChanged();
}

void TorrentSearchBackend::addRecentSearch(const QString &query)
{
    m_recentSearches.removeAll(query);
    m_recentSearches.prepend(query);
    while (m_recentSearches.size() > 10) {
        m_recentSearches.removeLast();
    }
    m_settings.setValue(QStringLiteral("recentSearches"), m_recentSearches);
    m_settings.sync();
    Q_EMIT recentSearchesChanged();
}

void TorrentSearchBackend::setStatusMessage(const QString &msg)
{
    if (m_statusMessage == msg) {
        return;
    }
    m_statusMessage = msg;
    Q_EMIT statusMessageChanged();
}
