/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import CachyOS.UI

Rectangle {
    id: root

    required property string toastText
    required property bool toastError

    visible: root.toastText.length > 0
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 22
    anchors.horizontalCenter: parent.horizontalCenter
    height: 36
    width: Math.min(parent.width - 48, toastLabel.implicitWidth + 36)
    radius: 18
    color: root.toastError ? "#FF3B30" : (CachyTheme.isDark ? Qt.rgba(0.12, 0.14, 0.18, 0.94) : Qt.rgba(1, 1, 1, 0.95))
    border.width: 1
    border.color: root.toastError ? "#FF6961" : CachyTheme.accentColor

    Text {
        id: toastLabel
        anchors.centerIn: parent
        width: parent.width - 28
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: root.toastText
        font.pixelSize: 12
        font.weight: Font.DemiBold
        color: root.toastError ? "#FFFFFF" : CachyTheme.textPrimary
    }
}
