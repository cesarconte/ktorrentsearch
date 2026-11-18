/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import CachyOS.UI

ListView {
    id: root

    visible: torrentBackend.totalResultsCount > 0 || torrentBackend.isSearching
    clip: true
    spacing: 8
    boundsBehavior: Flickable.StopAtBounds
    model: torrentBackend.resultsModel

    QQC2.ScrollBar.vertical: QQC2.ScrollBar {
        policy: QQC2.ScrollBar.AsNeeded
    }

    delegate: TorrentResultRow {
        width: root.width - 10
    }
}
