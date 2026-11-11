/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "torrentsearchbackend.h"

#include <KIO/CommandLauncherJob>

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QXmlStreamReader>

namespace {

/// Python 3.14 no longer finds the system CA bundle on its own, which breaks HTTPS for some engines
/// (e.g. TorrentProject: CERTIFICATE_VERIFY_FAILED). Point it at the distribution bundle when available.
QProcessEnvironment pythonEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.contains(QStringLiteral("SSL_CERT_FILE"))) {
        for (const QString &bundle : {QStringLiteral("/etc/ssl/certs/ca-certificates.crt"),
                                      QStringLiteral("/etc/ca-certificates/extracted/tls-ca-bundle.pem")}) {
            if (QFileInfo::exists(bundle)) {
                env.insert(QStringLiteral("SSL_CERT_FILE"), bundle);
                break;
            }
        }
    }
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    return env;
}

/// Copies files from srcDirPath to dstDirPath. A file that already exists at the destination is
/// only replaced when `overwrite` is set, or when the source carries a newer "# VERSION:" header.
void copyDirectoryFiles(const QString &srcDirPath, const QString &dstDirPath, bool overwrite = false)
{
    QDir srcDir(srcDirPath);
    if (!srcDir.exists()) {
        return;
    }

    QDir dstDir(dstDirPath);
    if (!dstDir.exists()) {
        dstDir.mkpath(QStringLiteral("."));
    }

    const QFileInfoList entries = srcDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        const QString dstFile = dstDir.filePath(fi.fileName());
        if (QFile::exists(dstFile)
            && (overwrite
                || (fi.suffix() == QLatin1String("py")
                    && TorrentSearchBackend::compareVersions(
                           TorrentSearchBackend::pluginVersion(fi.absoluteFilePath()),
                           TorrentSearchBackend::pluginVersion(dstFile))
                        > 0))) {
            QFile::remove(dstFile);
        }
        if (!QFile::exists(dstFile)) {
            if (QFile::copy(fi.absoluteFilePath(), dstFile)) {
                QFile::setPermissions(dstFile,
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                          | QFileDevice::ReadGroup | QFileDevice::ReadOther);
            }
        }
    }
}

} // namespace

TorrentSearchBackend::TorrentSearchBackend(QObject *parent)
    : QObject(parent)
    , m_sourceModel(this)
    , m_proxyModel(this)
    , m_settings(QStringLiteral("Cesar"), QStringLiteral("ktorrentsearch"))
{
    m_proxyModel.setSourceModel(&m_sourceModel);

    connect(&m_sourceModel, &TorrentListModel::countChanged,
            this, &TorrentSearchBackend::totalResultsCountChanged);
    connect(&m_proxyModel, &TorrentFilterProxyModel::countChanged,
            this, &TorrentSearchBackend::filteredResultsCountChanged);

    m_transmissionInstalled = !QStandardPaths::findExecutable(QStringLiteral("transmission-qt")).isEmpty();
    m_recentSearches = m_settings.value(QStringLiteral("recentSearches")).toStringList();

    ensureNovaRuntime();
    loadEnginesCapabilities();

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

QVariantList TorrentSearchBackend::engines() const
{
    QVariantList list;
    list.reserve(m_engines.size());
    for (const SearchEngineInfo &eng : m_engines) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), eng.id);
        map.insert(QStringLiteral("name"), eng.name);
        map.insert(QStringLiteral("url"), eng.url);
        map.insert(QStringLiteral("categories"), eng.categories);
        map.insert(QStringLiteral("enabled"), eng.enabled);
        map.insert(QStringLiteral("resultCount"), eng.resultCount);
        map.insert(QStringLiteral("running"), eng.running);
        list.push_back(map);
    }
    return list;
}

int TorrentSearchBackend::enabledEnginesCount() const
{
    int count = 0;
    for (const SearchEngineInfo &eng : m_engines) {
        if (eng.enabled) {
            ++count;
        }
    }
    return count;
}

void TorrentSearchBackend::ensureNovaRuntime()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/ktorrentsearch/nova3");
    m_novaDir = appData;

    QDir().mkpath(m_novaDir + QStringLiteral("/engines"));

    // Candidate sources in priority order:
    // 1. Installed system/user share dir (~/.local/share/ktorrentsearch/nova3 or /usr/share/ktorrentsearch/nova3)
    // 2. Project source data/nova3 directory
    // 3. Existing qBittorrent nova3 directory (if still installed, to import any extra user plugins)
    const QStringList candidateRoots = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/../share/ktorrentsearch/nova3"),
        QStringLiteral("/home/cesar/Proyectos/ktorrentsearch/data/nova3"),
        QDir::homePath() + QStringLiteral("/.local/share/qBittorrent/nova3"),
    };

    for (const QString &srcRoot : candidateRoots) {
        if (QFileInfo::exists(srcRoot + QStringLiteral("/nova2.py"))) {
            copyDirectoryFiles(srcRoot, m_novaDir, false);
            copyDirectoryFiles(srcRoot + QStringLiteral("/engines"), m_novaDir + QStringLiteral("/engines"), false);
        }
    }
}

QVector<SearchEngineInfo> TorrentSearchBackend::parseCapabilitiesXml(const QString &xmlContent)
{
    QVector<SearchEngineInfo> parsed;
    if (xmlContent.trimmed().isEmpty()) {
        return parsed;
    }

    QXmlStreamReader xml(xmlContent);
    while (!xml.atEnd() && !xml.hasError()) {
        const QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QStringView(u"capabilities")) {
                continue;
            }

            SearchEngineInfo info;
            info.id = xml.name().toString().trimmed();
            if (info.id.isEmpty()) {
                continue;
            }

            while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == info.id)
                   && !xml.atEnd()) {
                xml.readNext();
                if (xml.tokenType() == QXmlStreamReader::StartElement) {
                    const QString tagName = xml.name().toString();
                    const QString text = xml.readElementText().trimmed();
                    if (tagName == QStringLiteral("name")) {
                        info.name = text;
                    } else if (tagName == QStringLiteral("url")) {
                        info.url = text;
                    } else if (tagName == QStringLiteral("categories")) {
                        info.categories = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    }
                }
            }

            if (info.name.isEmpty()) {
                info.name = info.id;
            }
            parsed.push_back(info);
        }
    }

    return parsed;
}

void TorrentSearchBackend::loadEnginesCapabilities()
{
    const QString novaScript = m_novaDir + QStringLiteral("/nova2.py");
    if (!QFileInfo::exists(novaScript)) {
        return;
    }

    QProcess proc;
    proc.setWorkingDirectory(m_novaDir);
    proc.setProcessEnvironment(pythonEnvironment());
    proc.start(QStringLiteral("python3"), {QStringLiteral("-I"), novaScript, QStringLiteral("--capabilities")});
    proc.waitForFinished(8000);

    const QString xmlOut = QString::fromUtf8(proc.readAllStandardOutput());
    QVector<SearchEngineInfo> discovered = parseCapabilitiesXml(xmlOut);

    for (SearchEngineInfo &eng : discovered) {
        // Default jackett to disabled unless configured, as it requires a local Jackett daemon on 127.0.0.1:9117
        const bool defaultEnabled = (eng.id != QStringLiteral("jackett"));
        eng.enabled = m_settings.value(QStringLiteral("engines/") + eng.id, defaultEnabled).toBool();
    }

    m_engines = discovered;
    Q_EMIT enginesChanged();
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

    QVector<int> activeIndices;
    for (int i = 0; i < m_engines.size(); ++i) {
        m_engines[i].resultCount = 0;
        m_engines[i].running = false;
        if (!m_engines[i].enabled) {
            continue;
        }
        // Check if engine supports selected category (empty categories list means general engine supporting all)
        if (m_currentCategory != QStringLiteral("all")
            && !m_engines[i].categories.isEmpty()
            && !m_engines[i].categories.contains(m_currentCategory)) {
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
        SearchEngineInfo &eng = m_engines[idx];
        eng.running = true;

        auto *proc = new QProcess(this);
        proc->setWorkingDirectory(m_novaDir);
        proc->setProcessEnvironment(pythonEnvironment());
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
                        for (SearchEngineInfo &e : m_engines) {
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
                        for (SearchEngineInfo &e : m_engines) {
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

            for (SearchEngineInfo &e : m_engines) {
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

        // 15-second per-engine timeout watchdog
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

    for (SearchEngineInfo &e : m_engines) {
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
    for (SearchEngineInfo &e : m_engines) {
        e.resultCount = 0;
    }
    setStatusMessage(QStringLiteral("Resultados limpiados"));
    Q_EMIT enginesChanged();
    Q_EMIT searchStateChanged();
}

void TorrentSearchBackend::downloadTorrent(int proxyRow)
{
    const QVariantMap row = m_proxyModel.getRow(proxyRow);
    if (row.isEmpty()) {
        return;
    }
    downloadByLink(row.value(QStringLiteral("link")).toString(),
                   row.value(QStringLiteral("engineId")).toString(),
                   row.value(QStringLiteral("name")).toString());
}

void TorrentSearchBackend::downloadByLink(const QString &link, const QString &engineId, const QString &title)
{
    const QString trimmedLink = link.trimmed();
    if (trimmedLink.isEmpty()) {
        return;
    }

    const QString displayTitle = title.isEmpty() ? trimmedLink.left(48) : title;

    // 1. Direct Magnet URI -> send straight to transmission-qt
    if (trimmedLink.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
        launchInTransmission(trimmedLink, displayTitle);
        return;
    }

    // 2. HTTP/HTTPS or Engine-specific download parameter -> resolve via nova2dl.py
    const QString novaDlScript = m_novaDir + QStringLiteral("/nova2dl.py");
    if (!QFileInfo::exists(novaDlScript) || engineId.isEmpty()) {
        launchInTransmission(trimmedLink, displayTitle);
        return;
    }

    setStatusMessage(QStringLiteral("Descargando archivo .torrent para «%1»…").arg(displayTitle));

    auto *dlProc = new QProcess(this);
    dlProc->setWorkingDirectory(m_novaDir);
    dlProc->setProcessEnvironment(pythonEnvironment());

    connect(dlProc, &QProcess::finished, this, [this, dlProc, trimmedLink, displayTitle](int exitCode, QProcess::ExitStatus) {
        const QString stdoutStr = QString::fromUtf8(dlProc->readAllStandardOutput()).trimmed();
        dlProc->deleteLater();

        if (exitCode == 0 && !stdoutStr.isEmpty()) {
            // nova2dl.py prints: "<tempfile_path> <url>" or "magnet:?xt=..."
            const QString firstToken = stdoutStr.section(QLatin1Char(' '), 0, 0).trimmed();
            if (firstToken.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
                launchInTransmission(firstToken, displayTitle);
                return;
            }
            if (QFileInfo::exists(firstToken)) {
                QString torrentPath = firstToken;
                if (!torrentPath.endsWith(QStringLiteral(".torrent"), Qt::CaseInsensitive)) {
                    const QString renamedPath = firstToken + QStringLiteral(".torrent");
                    QFile::remove(renamedPath);
                    if (QFile::rename(firstToken, renamedPath)) {
                        torrentPath = renamedPath;
                    }
                }
                launchInTransmission(torrentPath, displayTitle);
                return;
            }
        }

        // Fallback: pass URL directly to transmission-qt
        launchInTransmission(trimmedLink, displayTitle);
    });

    dlProc->start(QStringLiteral("python3"), {QStringLiteral("-I"), novaDlScript, engineId, trimmedLink});
}

void TorrentSearchBackend::launchInTransmission(const QString &targetUriOrPath, const QString &displayName)
{
    const QString transmissionBin = QStandardPaths::findExecutable(QStringLiteral("transmission-qt"));
    if (!transmissionBin.isEmpty()) {
        auto *job = new KIO::CommandLauncherJob(QStringLiteral("transmission-qt"), {targetUriOrPath}, this);
        job->setDesktopName(QStringLiteral("transmission-qt"));
        connect(job, &KJob::result, this, [this, job, displayName, targetUriOrPath]() {
            if (job->error()) {
                QProcess::startDetached(QStringLiteral("transmission-qt"), {targetUriOrPath});
            }
            setStatusMessage(QStringLiteral("Enviado a Transmission Qt: «%1»").arg(displayName));
            Q_EMIT toastNotification(QStringLiteral("Añadido a Transmission Qt: %1").arg(displayName), false);
        });
        job->start();
        return;
    }

    // Fallback to xdg-open if transmission-qt is not found
    QDesktopServices::openUrl(QUrl::fromUserInput(targetUriOrPath));
    setStatusMessage(QStringLiteral("Abierto con gestor predeterminado: «%1»").arg(displayName));
    Q_EMIT toastNotification(QStringLiteral("Abierto enlace: %1").arg(displayName), false);
}

void TorrentSearchBackend::copyMagnetOrLink(const QString &link)
{
    if (link.trimmed().isEmpty()) {
        return;
    }
    if (QClipboard *cb = QGuiApplication::clipboard()) {
        cb->setText(link.trimmed());
        setStatusMessage(QStringLiteral("Enlace magnet/torrent copiado al portapapeles"));
        Q_EMIT toastNotification(QStringLiteral("Enlace copiado al portapapeles"), false);
    }
}

void TorrentSearchBackend::openDescriptionPage(const QString &descLink)
{
    const QString trimmed = descLink.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromUserInput(trimmed));
}

void TorrentSearchBackend::openTransmission()
{
    auto *job = new KIO::CommandLauncherJob(QStringLiteral("transmission-qt"), {}, this);
    job->setDesktopName(QStringLiteral("transmission-qt"));
    connect(job, &KJob::result, this, [job]() {
        if (job->error()) {
            QProcess::startDetached(QStringLiteral("transmission-qt"), {});
        }
    });
    job->start();
}

void TorrentSearchBackend::setEngineEnabled(const QString &engineId, bool enabled)
{
    for (SearchEngineInfo &eng : m_engines) {
        if (eng.id == engineId) {
            if (eng.enabled == enabled) {
                return;
            }
            eng.enabled = enabled;
            m_settings.setValue(QStringLiteral("engines/") + engineId, enabled);
            m_settings.sync();
            Q_EMIT enginesChanged();
            return;
        }
    }
}

void TorrentSearchBackend::setAllEnginesEnabled(bool enabled)
{
    for (SearchEngineInfo &eng : m_engines) {
        // Keep jackett opt-in when enabling all unless explicitly clicked
        if (enabled && eng.id == QStringLiteral("jackett")) {
            continue;
        }
        eng.enabled = enabled;
        m_settings.setValue(QStringLiteral("engines/") + eng.id, enabled);
    }
    m_settings.sync();
    Q_EMIT enginesChanged();
}

bool TorrentSearchBackend::installPluginFromFile(const QString &fileUrlOrPath)
{
    QString localPath = fileUrlOrPath.trimmed();
    if (localPath.startsWith(QStringLiteral("file://"))) {
        localPath = QUrl(localPath).toLocalFile();
    }

    QFileInfo fi(localPath);
    if (!fi.exists() || fi.suffix().toLower() != QStringLiteral("py")) {
        setStatusMessage(QStringLiteral("Selecciona un archivo de plugin válido (.py)."));
        Q_EMIT toastNotification(m_statusMessage, true);
        return false;
    }

    const QString dstPath = m_novaDir + QStringLiteral("/engines/") + fi.fileName();
    if (QFile::exists(dstPath)) {
        QFile::remove(dstPath);
    }
    if (!QFile::copy(fi.absoluteFilePath(), dstPath)) {
        setStatusMessage(QStringLiteral("No se pudo copiar el plugin a ") + dstPath);
        Q_EMIT toastNotification(m_statusMessage, true);
        return false;
    }

    QFile::setPermissions(dstPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ReadGroup | QFileDevice::ReadOther);

    loadEnginesCapabilities();
    setStatusMessage(QStringLiteral("Plugin «%1» instalado correctamente").arg(fi.baseName()));
    Q_EMIT toastNotification(m_statusMessage, false);
    return true;
}

QString TorrentSearchBackend::pluginVersion(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return versionFromHeader(file.read(256));
}

QString TorrentSearchBackend::versionFromHeader(const QByteArray &head)
{
    static const QRegularExpression re(QStringLiteral("^#\\s*VERSION:\\s*([0-9][0-9.]*)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QString firstLine = QString::fromUtf8(head.left(head.indexOf('\n'))).trimmed();
    const auto match = re.match(firstLine);
    return match.hasMatch() ? match.captured(1) : QString();
}

int TorrentSearchBackend::compareVersions(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    const QStringList pb = b.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa.at(i).toInt() : 0;
        const int vb = i < pb.size() ? pb.at(i).toInt() : 0;
        if (va != vb) {
            return va < vb ? -1 : 1;
        }
    }
    return 0;
}

QUrl TorrentSearchBackend::upstreamPluginUrl(const QString &engineId)
{
    // Unofficial community plugins (qBittorrent wiki "Unofficial search plugins")
    static const QHash<QString, QString> unofficial = {
        {QStringLiteral("therarbg"),
         QStringLiteral("https://raw.githubusercontent.com/BurningMop/qBittorrent-Search-Plugins/main/therarbg.py")},
        {QStringLiteral("kickasstorrents"),
         QStringLiteral("https://raw.githubusercontent.com/LightDestory/qBittorrent-Search-Plugins/master/src/engines/kickasstorrents.py")},
    };
    const auto it = unofficial.constFind(engineId);
    if (it != unofficial.constEnd()) {
        return QUrl(it.value());
    }
    // Official plugins repository maintained by the qBittorrent project
    return QUrl(QStringLiteral("https://raw.githubusercontent.com/qbittorrent/search-plugins/master/nova3/engines/%1.py")
                    .arg(engineId));
}

void TorrentSearchBackend::updateEngines()
{
    if (m_updatingEngines) {
        return;
    }
    if (m_engines.isEmpty()) {
        setStatusMessage(QStringLiteral("No hay motores instalados que actualizar."));
        return;
    }

    m_updatingEngines = true;
    m_updatePending = m_engines.size();
    m_updatedEngines.clear();
    m_updateFailures = 0;
    Q_EMIT updatingEnginesChanged();
    setStatusMessage(QStringLiteral("Comprobando actualizaciones de %1 motores…").arg(m_engines.size()));

    for (const SearchEngineInfo &eng : std::as_const(m_engines)) {
        const QString engineId = eng.id;
        const QString engineName = eng.name;
        QNetworkRequest req(upstreamPluginUrl(engineId));
        req.setTransferTimeout(12000);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_network.get(req);

        connect(reply, &QNetworkReply::finished, this, [this, reply, engineId, engineName]() {
            reply->deleteLater();
            const QByteArray body = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();

            const QString localPath = m_novaDir + QStringLiteral("/engines/") + engineId + QStringLiteral(".py");
            const QString remoteVersion = versionFromHeader(body);
            const bool looksValid = !remoteVersion.isEmpty()
                && body.contains(QByteArray("class " + engineId.toUtf8()));

            if (!looksValid) {
                // 404 / network error / not a nova3 plugin: engine has no reachable upstream, keep local copy.
                if (reply->error() != QNetworkReply::NoError
                    && reply->error() != QNetworkReply::ContentNotFoundError) {
                    ++m_updateFailures;
                }
            } else if (compareVersions(remoteVersion, pluginVersion(localPath)) > 0) {
                QSaveFile out(localPath);
                if (out.open(QIODevice::WriteOnly) && out.write(body) == body.size() && out.commit()) {
                    m_updatedEngines.append(QStringLiteral("%1 %2").arg(engineName, remoteVersion));
                } else {
                    ++m_updateFailures;
                }
            }

            if (--m_updatePending > 0) {
                return;
            }

            m_updatingEngines = false;
            Q_EMIT updatingEnginesChanged();
            loadEnginesCapabilities();

            QString summary;
            if (!m_updatedEngines.isEmpty()) {
                summary = QStringLiteral("Motores actualizados: %1").arg(m_updatedEngines.join(QStringLiteral(", ")));
            } else if (m_updateFailures > 0) {
                summary = QStringLiteral("No se pudo comprobar %1 motor(es). Revisa tu conexión.").arg(m_updateFailures);
            } else {
                summary = QStringLiteral("Todos los motores ya están actualizados.");
            }
            setStatusMessage(summary);
            Q_EMIT toastNotification(summary, m_updatedEngines.isEmpty() && m_updateFailures > 0);
        });
    }
}

void TorrentSearchBackend::refreshEngines()
{
    ensureNovaRuntime();
    loadEnginesCapabilities();
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
