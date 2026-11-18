/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import CachyOS.UI

Item {
    id: root

    signal recentSearchSelected(string query)

    visible: torrentBackend.totalResultsCount === 0 && !torrentBackend.isSearching

    CachyCard {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 540)

        contentItem: ColumnLayout {
            spacing: 14

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 52
                Layout.preferredHeight: 52
                radius: 16
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#0A84FF" }
                    GradientStop { position: 1.0; color: "#30D158" }
                }

                Text {
                    anchors.centerIn: parent
                    text: "🧲"
                    font.pixelSize: 24
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "Buscador Multi-Tracker Nativo para Transmission Qt"
                font.pixelSize: 16
                font.weight: Font.Bold
                color: CachyTheme.textPrimary
            }

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "Ejecuta los motores nova3 de forma autónoma y paralela (sin depender de qBittorrent) y envía enlaces Magnet o archivos .torrent directamente a Transmission Qt con un solo clic."
                font.pixelSize: 13
                color: CachyTheme.textSecondary
            }

            // Recent searches chips
            ColumnLayout {
                visible: torrentBackend.recentSearches.length > 0
                Layout.fillWidth: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: "Búsquedas recientes:"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: CachyTheme.textMuted
                    }
                    Text {
                        text: "Borrar historial"
                        font.pixelSize: 11
                        color: CachyTheme.accentColor
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: torrentBackend.clearRecentSearches() }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: torrentBackend.recentSearches

                        Rectangle {
                            id: chipRect
                            required property string modelData
                            height: 26
                            implicitWidth: recentLabel.implicitWidth + 18
                            radius: 13
                            color: CachyTheme.surfacePill
                            border.width: 1
                            border.color: CachyTheme.specularBorder

                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: root.recentSearchSelected(chipRect.modelData)
                            }

                            Text {
                                id: recentLabel
                                anchors.centerIn: parent
                                text: "🕒 " + chipRect.modelData
                                font.pixelSize: 12
                                color: CachyTheme.textPrimary
                            }
                        }
                    }
                }
            }
        }
    }
}
