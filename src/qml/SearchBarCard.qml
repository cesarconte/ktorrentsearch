/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import CachyOS.UI

CachyCard {
    id: root

    topPadding: 10
    bottomPadding: 10
    leftPadding: 12
    rightPadding: 12

    function focusSearch() {
        queryInput.forceActiveFocus();
    }

    function setQueryText(text) {
        queryInput.text = text;
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            CachyTextField {
                id: queryInput
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                isSearch: true
                placeholderText: "Buscar torrents en la red (ej. CachyOS, Ubuntu 24.04, Blender, Linux)…"
                onAccepted: {
                    if (text.trim().length > 0) {
                        torrentBackend.search(text.trim());
                    }
                }
                Component.onCompleted: forceActiveFocus()
            }

            CachyButton {
                primary: !torrentBackend.isSearching
                destructive: torrentBackend.isSearching
                text: torrentBackend.isSearching ? "⏹ Detener" : "🔍 Buscar"
                onClicked: {
                    if (torrentBackend.isSearching) {
                        torrentBackend.stopSearch();
                    } else {
                        torrentBackend.search(queryInput.text);
                    }
                }
            }

            CachyButton {
                visible: torrentBackend.totalResultsCount > 0 && !torrentBackend.isSearching
                text: "Limpiar"
                onClicked: torrentBackend.clearResults()
            }
        }

        // Progress Bar + Live Engine Status + Sort Controls
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            CachyProgressBar {
                visible: torrentBackend.isSearching
                Layout.preferredWidth: 120
                from: 0.0
                to: 1.0
                value: torrentBackend.searchProgress
            }

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: torrentBackend.statusMessage
                font.pixelSize: 12
                color: CachyTheme.textSecondary
                elide: Text.ElideRight
            }

            // Sort Pill Selector
            RowLayout {
                spacing: 4

                Text {
                    text: "Ordenar:"
                    font.pixelSize: 11
                    color: CachyTheme.textMuted
                }

                Repeater {
                    model: [
                        { key: "seeds",    label: "Semillas" },
                        { key: "size",     label: "Tamaño" },
                        { key: "leechers", label: "Leechers" },
                        { key: "date",     label: "Fecha" },
                        { key: "name",     label: "Nombre" }
                    ]

                    Rectangle {
                        id: sortChipRect
                        required property var modelData
                        readonly property bool isCurrentSort: torrentBackend.resultsModel.sortField === modelData.key

                        height: 24
                        implicitWidth: sortChipText.implicitWidth + 14
                        radius: 12
                        color: isCurrentSort
                            ? CachyTheme.withAlpha(CachyTheme.accentColor, 0.22)
                            : CachyTheme.surfacePill
                        border.width: 1
                        border.color: isCurrentSort ? CachyTheme.accentColor : CachyTheme.specularBorder

                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: {
                                if (sortChipRect.isCurrentSort) {
                                    torrentBackend.resultsModel.sortDescending = !torrentBackend.resultsModel.sortDescending;
                                } else {
                                    torrentBackend.resultsModel.sortField = sortChipRect.modelData.key;
                                    torrentBackend.resultsModel.sortDescending = (sortChipRect.modelData.key !== "name");
                                }
                            }
                        }

                        Text {
                            id: sortChipText
                            anchors.centerIn: parent
                            text: sortChipRect.modelData.label + (sortChipRect.isCurrentSort ? (torrentBackend.resultsModel.sortDescending ? " ↓" : " ↑") : "")
                            font.pixelSize: 11
                            font.weight: sortChipRect.isCurrentSort ? Font.Bold : Font.Medium
                            color: sortChipRect.isCurrentSort ? CachyTheme.accentColor : CachyTheme.textSecondary
                        }
                    }
                }
            }
        }
    }
}
