/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import CachyOS.UI

Rectangle {
    id: root

    required property int index
    required property string link
    required property string name
    required property string formattedSize
    required property int seeds
    required property int leechers
    required property string engineId
    required property string engineName
    required property string descLink
    required property string formattedDate
    required property bool isMagnet

    implicitHeight: cardLayout.implicitHeight + 22
    radius: CachyTheme.radiusMedium
    color: rowHover.hovered ? CachyTheme.cardBackgroundHover : CachyTheme.cardBackground
    border.width: 1
    border.color: rowHover.hovered ? CachyTheme.withAlpha(CachyTheme.accentColor, 0.45) : CachyTheme.specularBorder

    Behavior on color { ColorAnimation { duration: 120 } }

    HoverHandler {
        id: rowHover
        cursorShape: Qt.PointingHandCursor
    }

    // Double click sends directly to Transmission Qt
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: {
            torrentBackend.downloadByLink(root.link, root.engineId, root.name);
        }
    }

    // Right click context menu
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: rowContextMenu.popup()
    }

    QQC2.Menu {
        id: rowContextMenu

        QQC2.MenuItem {
            icon.name: "download"
            text: "Descargar en Transmission Qt"
            onTriggered: torrentBackend.downloadByLink(root.link, root.engineId, root.name)
        }

        QQC2.MenuItem {
            icon.name: "edit-copy"
            text: root.isMagnet ? "Copiar enlace Magnet" : "Copiar enlace del torrent"
            onTriggered: torrentBackend.copyMagnetOrLink(root.link)
        }

        QQC2.MenuItem {
            enabled: root.descLink.length > 0
            icon.name: "internet-web-browser"
            text: "Abrir página de descripción en el navegador"
            onTriggered: torrentBackend.openDescriptionPage(root.descLink)
        }
    }

    RowLayout {
        id: cardLayout
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 11
        anchors.bottomMargin: 11
        spacing: 12

        // 1. Left Type Badge (Magnet vs .torrent)
        Rectangle {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: 10
            color: root.isMagnet
                ? Qt.rgba(0.04, 0.52, 1.0, 0.16)
                : Qt.rgba(0.75, 0.35, 0.95, 0.16)
            border.width: 1
            border.color: root.isMagnet
                ? Qt.rgba(0.04, 0.52, 1.0, 0.35)
                : Qt.rgba(0.75, 0.35, 0.95, 0.35)

            Text {
                anchors.centerIn: parent
                text: root.isMagnet ? "🧲" : "📦"
                font.pixelSize: 16
            }
        }

        // 2. Center Title + Metadata Badges (Strict Zero-Overflow rules)
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 5

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.name
                color: CachyTheme.textPrimary
                font.pixelSize: 13
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 8

                // Engine Source Pill
                Rectangle {
                    Layout.preferredHeight: 20
                    implicitWidth: Math.min(220, engLabel.implicitWidth + 14)
                    radius: 6
                    color: CachyTheme.surfacePill
                    border.width: 1
                    border.color: CachyTheme.specularBorder

                    Text {
                        id: engLabel
                        anchors.centerIn: parent
                        width: parent.width - 12
                        horizontalAlignment: Text.AlignHCenter
                        text: root.engineName
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: CachyTheme.accentColor
                        elide: Text.ElideRight
                    }
                }

                // Size Badge
                Text {
                    text: "💾 " + root.formattedSize
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: CachyTheme.textSecondary
                }

                // Date Badge
                Text {
                    visible: root.formattedDate !== "—"
                    text: "📅 " + root.formattedDate
                    font.pixelSize: 11
                    color: CachyTheme.textMuted
                }

                Item { Layout.fillWidth: true }
            }
        }

        // 3. Seeders & Leechers Pills
        RowLayout {
            spacing: 6

            // Seeders Pill
            Rectangle {
                Layout.preferredHeight: 26
                implicitWidth: Math.max(58, seedsText.implicitWidth + 16)
                radius: 8
                color: root.seeds > 0
                    ? Qt.rgba(0.19, 0.82, 0.35, 0.16)
                    : Qt.rgba(0.56, 0.56, 0.58, 0.14)
                border.width: 1
                border.color: root.seeds > 0
                    ? Qt.rgba(0.19, 0.82, 0.35, 0.42)
                    : CachyTheme.specularBorder

                Text {
                    id: seedsText
                    anchors.centerIn: parent
                    text: "▲ " + root.seeds
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: root.seeds > 0 ? "#30D158" : CachyTheme.textMuted
                }
            }

            // Leechers Pill
            Rectangle {
                Layout.preferredHeight: 26
                implicitWidth: Math.max(52, leechText.implicitWidth + 16)
                radius: 8
                color: Qt.rgba(0.04, 0.52, 1.0, 0.12)
                border.width: 1
                border.color: Qt.rgba(0.04, 0.52, 1.0, 0.28)

                Text {
                    id: leechText
                    anchors.centerIn: parent
                    text: "▼ " + root.leechers
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: "#409CFF"
                }
            }
        }

        // 4. Direct Action Buttons (Download in Transmission, Copy Link, Open Web)
        RowLayout {
            spacing: 6

            CachyToolbarButton {
                text: "🔗"
                tooltipText: root.isMagnet ? "Copiar enlace Magnet al portapapeles" : "Copiar URL del torrent"
                onClicked: torrentBackend.copyMagnetOrLink(root.link)
            }

            CachyToolbarButton {
                visible: root.descLink.length > 0
                text: "🌐"
                tooltipText: "Abrir página del torrent en el navegador"
                onClicked: torrentBackend.openDescriptionPage(root.descLink)
            }

            CachyButton {
                primary: true
                text: "⬇ Descargar"
                onClicked: torrentBackend.downloadByLink(root.link, root.engineId, root.name)
            }
        }
    }
}
