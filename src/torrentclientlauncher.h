/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "torrentmodel.h"

#include <QObject>
#include <QString>

class TorrentClientLauncher : public QObject
{
    Q_OBJECT

public:
    explicit TorrentClientLauncher(QObject *parent = nullptr);
    ~TorrentClientLauncher() override = default;

    bool isTransmissionInstalled() const;

    void downloadTorrent(int proxyRow, TorrentFilterProxyModel *proxyModel, const QString &novaDir);
    void downloadByLink(const QString &link, const QString &engineId, const QString &title, const QString &novaDir);
    void launchInTransmission(const QString &targetUriOrPath, const QString &displayName);
    void copyMagnetOrLink(const QString &link);
    void openDescriptionPage(const QString &descLink);
    void openTransmission();

Q_SIGNALS:
    void statusMessageRequested(const QString &msg);
    void toastRequested(const QString &msg, bool isError);
};
