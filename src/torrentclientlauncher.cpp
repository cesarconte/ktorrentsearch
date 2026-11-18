/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "torrentclientlauncher.h"
#include "torrentenginemanager.h"

#include <KIO/CommandLauncherJob>

#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

TorrentClientLauncher::TorrentClientLauncher(QObject *parent)
    : QObject(parent)
{
}

bool TorrentClientLauncher::isTransmissionInstalled() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("transmission-qt")).isEmpty();
}

void TorrentClientLauncher::downloadTorrent(int proxyRow, TorrentFilterProxyModel *proxyModel, const QString &novaDir)
{
    if (!proxyModel) {
        return;
    }
    const QVariantMap row = proxyModel->getRow(proxyRow);
    if (row.isEmpty()) {
        return;
    }
    downloadByLink(row.value(QStringLiteral("link")).toString(),
                   row.value(QStringLiteral("engineId")).toString(),
                   row.value(QStringLiteral("name")).toString(),
                   novaDir);
}

void TorrentClientLauncher::downloadByLink(const QString &link, const QString &engineId, const QString &title, const QString &novaDir)
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
    const QString novaDlScript = novaDir + QStringLiteral("/nova2dl.py");
    if (!QFileInfo::exists(novaDlScript) || engineId.isEmpty()) {
        launchInTransmission(trimmedLink, displayTitle);
        return;
    }

    Q_EMIT statusMessageRequested(QStringLiteral("Descargando archivo .torrent para «%1»…").arg(displayTitle));

    auto *dlProc = new QProcess(this);
    dlProc->setWorkingDirectory(novaDir);
    dlProc->setProcessEnvironment(TorrentEngineManager::pythonEnvironment());

    connect(dlProc, &QProcess::finished, this, [this, dlProc, trimmedLink, displayTitle](int exitCode, QProcess::ExitStatus) {
        const QString stdoutStr = QString::fromUtf8(dlProc->readAllStandardOutput()).trimmed();
        dlProc->deleteLater();

        if (exitCode == 0 && !stdoutStr.isEmpty()) {
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

void TorrentClientLauncher::launchInTransmission(const QString &targetUriOrPath, const QString &displayName)
{
    const QString transmissionBin = QStandardPaths::findExecutable(QStringLiteral("transmission-qt"));
    if (!transmissionBin.isEmpty()) {
        auto *job = new KIO::CommandLauncherJob(QStringLiteral("transmission-qt"), {targetUriOrPath}, this);
        job->setDesktopName(QStringLiteral("transmission-qt"));
        connect(job, &KJob::result, this, [this, job, displayName, targetUriOrPath]() {
            if (job->error()) {
                QProcess::startDetached(QStringLiteral("transmission-qt"), {targetUriOrPath});
            }
            const QString msg = QStringLiteral("Enviado a Transmission Qt: «%1»").arg(displayName);
            Q_EMIT statusMessageRequested(msg);
            Q_EMIT toastRequested(QStringLiteral("Añadido a Transmission Qt: %1").arg(displayName), false);
        });
        job->start();
        return;
    }

    // Fallback to xdg-open if transmission-qt is not found
    QDesktopServices::openUrl(QUrl::fromUserInput(targetUriOrPath));
    const QString msg = QStringLiteral("Abierto con gestor predeterminado: «%1»").arg(displayName);
    Q_EMIT statusMessageRequested(msg);
    Q_EMIT toastRequested(QStringLiteral("Abierto enlace: %1").arg(displayName), false);
}

void TorrentClientLauncher::copyMagnetOrLink(const QString &link)
{
    if (link.trimmed().isEmpty()) {
        return;
    }
    if (QClipboard *cb = QGuiApplication::clipboard()) {
        cb->setText(link.trimmed());
        Q_EMIT statusMessageRequested(QStringLiteral("Enlace magnet/torrent copiado al portapapeles"));
        Q_EMIT toastRequested(QStringLiteral("Enlace copiado al portapapeles"), false);
    }
}

void TorrentClientLauncher::openDescriptionPage(const QString &descLink)
{
    const QString trimmed = descLink.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromUserInput(trimmed));
}

void TorrentClientLauncher::openTransmission()
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
