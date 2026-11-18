/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "torrentenginemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QXmlStreamReader>

TorrentEngineManager::TorrentEngineManager(QObject *parent)
    : QObject(parent)
{
}

QProcessEnvironment TorrentEngineManager::pythonEnvironment()
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

void TorrentEngineManager::copyDirectoryFiles(const QString &srcDirPath, const QString &dstDirPath, bool overwrite)
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
                    && compareVersions(pluginVersion(fi.absoluteFilePath()), pluginVersion(dstFile)) > 0))) {
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

QVector<SearchEngineInfo> TorrentEngineManager::parseCapabilitiesXml(const QString &xmlContent)
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

QString TorrentEngineManager::pluginVersion(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return versionFromHeader(file.read(256));
}

QString TorrentEngineManager::versionFromHeader(const QByteArray &head)
{
    static const QRegularExpression re(QStringLiteral("^#\\s*VERSION:\\s*([0-9][0-9.]*)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QString firstLine = QString::fromUtf8(head.left(head.indexOf('\n'))).trimmed();
    const auto match = re.match(firstLine);
    return match.hasMatch() ? match.captured(1) : QString();
}

int TorrentEngineManager::compareVersions(const QString &a, const QString &b)
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

QUrl TorrentEngineManager::upstreamPluginUrl(const QString &engineId)
{
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
    return QUrl(QStringLiteral("https://raw.githubusercontent.com/qbittorrent/search-plugins/master/nova3/engines/%1.py")
                    .arg(engineId));
}

void TorrentEngineManager::ensureNovaRuntime(const QString &novaDir)
{
    QDir().mkpath(novaDir + QStringLiteral("/engines"));

    const QStringList candidateRoots = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/../share/ktorrentsearch/nova3"),
        QStringLiteral("/home/cesar/Proyectos/ktorrentsearch/data/nova3"),
        QDir::homePath() + QStringLiteral("/.local/share/qBittorrent/nova3"),
    };

    for (const QString &srcRoot : candidateRoots) {
        if (QFileInfo::exists(srcRoot + QStringLiteral("/nova2.py"))) {
            copyDirectoryFiles(srcRoot, novaDir, false);
            copyDirectoryFiles(srcRoot + QStringLiteral("/engines"), novaDir + QStringLiteral("/engines"), false);
        }
    }
}

void TorrentEngineManager::loadEnginesCapabilities(const QString &novaDir, QSettings &settings)
{
    const QString novaScript = novaDir + QStringLiteral("/nova2.py");
    if (!QFileInfo::exists(novaScript)) {
        return;
    }

    QProcess proc;
    proc.setWorkingDirectory(novaDir);
    proc.setProcessEnvironment(pythonEnvironment());
    proc.start(QStringLiteral("python3"), {QStringLiteral("-I"), novaScript, QStringLiteral("--capabilities")});
    proc.waitForFinished(8000);

    const QString xmlOut = QString::fromUtf8(proc.readAllStandardOutput());
    QVector<SearchEngineInfo> discovered = parseCapabilitiesXml(xmlOut);

    for (SearchEngineInfo &eng : discovered) {
        const bool defaultEnabled = (eng.id != QStringLiteral("jackett"));
        eng.enabled = settings.value(QStringLiteral("engines/") + eng.id, defaultEnabled).toBool();
    }

    m_engines = discovered;
    Q_EMIT enginesChanged();
}

QVariantList TorrentEngineManager::enginesVariantList() const
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

int TorrentEngineManager::enabledEnginesCount() const
{
    int count = 0;
    for (const SearchEngineInfo &eng : m_engines) {
        if (eng.enabled) {
            ++count;
        }
    }
    return count;
}

void TorrentEngineManager::setEngineEnabled(const QString &engineId, bool enabled, QSettings &settings)
{
    for (SearchEngineInfo &eng : m_engines) {
        if (eng.id == engineId) {
            if (eng.enabled == enabled) {
                return;
            }
            eng.enabled = enabled;
            settings.setValue(QStringLiteral("engines/") + engineId, enabled);
            settings.sync();
            Q_EMIT enginesChanged();
            return;
        }
    }
}

void TorrentEngineManager::setAllEnginesEnabled(bool enabled, QSettings &settings)
{
    for (SearchEngineInfo &eng : m_engines) {
        if (enabled && eng.id == QStringLiteral("jackett")) {
            continue;
        }
        eng.enabled = enabled;
        settings.setValue(QStringLiteral("engines/") + eng.id, enabled);
    }
    settings.sync();
    Q_EMIT enginesChanged();
}

bool TorrentEngineManager::installPluginFromFile(const QString &fileUrlOrPath, const QString &novaDir)
{
    QString localPath = fileUrlOrPath.trimmed();
    if (localPath.startsWith(QStringLiteral("file://"))) {
        localPath = QUrl(localPath).toLocalFile();
    }

    QFileInfo fi(localPath);
    if (!fi.exists() || fi.suffix().toLower() != QStringLiteral("py")) {
        const QString msg = QStringLiteral("Selecciona un archivo de plugin válido (.py).");
        Q_EMIT statusMessageRequested(msg);
        Q_EMIT toastRequested(msg, true);
        return false;
    }

    const QString dstPath = novaDir + QStringLiteral("/engines/") + fi.fileName();
    if (QFile::exists(dstPath)) {
        QFile::remove(dstPath);
    }
    if (!QFile::copy(fi.absoluteFilePath(), dstPath)) {
        const QString msg = QStringLiteral("No se pudo copiar el plugin a ") + dstPath;
        Q_EMIT statusMessageRequested(msg);
        Q_EMIT toastRequested(msg, true);
        return false;
    }

    QFile::setPermissions(dstPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ReadGroup | QFileDevice::ReadOther);

    const QString msg = QStringLiteral("Plugin «%1» instalado correctamente").arg(fi.baseName());
    Q_EMIT statusMessageRequested(msg);
    Q_EMIT toastRequested(msg, false);
    return true;
}

void TorrentEngineManager::updateEngines(const QString &novaDir, QSettings &settings)
{
    if (m_updatingEngines) {
        return;
    }
    if (m_engines.isEmpty()) {
        Q_EMIT statusMessageRequested(QStringLiteral("No hay motores instalados que actualizar."));
        return;
    }

    m_updatingEngines = true;
    m_updatePending = m_engines.size();
    m_updatedEngines.clear();
    m_updateFailures = 0;
    Q_EMIT updatingEnginesChanged();
    Q_EMIT statusMessageRequested(QStringLiteral("Comprobando actualizaciones de %1 motores…").arg(m_engines.size()));

    for (const SearchEngineInfo &eng : std::as_const(m_engines)) {
        const QString engineId = eng.id;
        const QString engineName = eng.name;
        QNetworkRequest req(upstreamPluginUrl(engineId));
        req.setTransferTimeout(12000);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_network.get(req);

        connect(reply, &QNetworkReply::finished, this, [this, reply, engineId, engineName, novaDir, &settings]() {
            reply->deleteLater();
            const QByteArray body = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();

            const QString localPath = novaDir + QStringLiteral("/engines/") + engineId + QStringLiteral(".py");
            const QString remoteVersion = versionFromHeader(body);
            const bool looksValid = !remoteVersion.isEmpty()
                && body.contains(QByteArray("class " + engineId.toUtf8()));

            if (!looksValid) {
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
            loadEnginesCapabilities(novaDir, settings);

            QString summary;
            if (!m_updatedEngines.isEmpty()) {
                summary = QStringLiteral("Motores actualizados: %1").arg(m_updatedEngines.join(QStringLiteral(", ")));
            } else if (m_updateFailures > 0) {
                summary = QStringLiteral("No se pudo comprobar %1 motor(es). Revisa tu conexión.").arg(m_updateFailures);
            } else {
                summary = QStringLiteral("Todos los motores ya están actualizados.");
            }
            Q_EMIT statusMessageRequested(summary);
            Q_EMIT toastRequested(summary, m_updatedEngines.isEmpty() && m_updateFailures > 0);
        });
    }
}
