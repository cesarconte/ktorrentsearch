/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import CachyOS.UI

ColumnLayout {
    id: root

    required property var categoryList
    required property bool showEnginesInspector
    signal toggleEnginesInspector()

    anchors.fill: parent
    anchors.leftMargin: 10
    anchors.rightMargin: 10
    anchors.bottomMargin: 12
    spacing: 3

    Text {
        Layout.leftMargin: 10
        Layout.topMargin: 4
        text: "Categorías"
        font.pixelSize: 11
        font.weight: Font.DemiBold
        color: CachyTheme.textMuted
    }

    Repeater {
        model: root.categoryList

        CachySidebarItem {
            required property var modelData
            Layout.fillWidth: true
            text: modelData.label
            iconGlyph: modelData.glyph
            iconColor: modelData.color
            squircleIconBadge: true
            badgeText: torrentBackend.currentCategory === modelData.id && torrentBackend.totalResultsCount > 0
                ? torrentBackend.totalResultsCount.toString()
                : ""
            selected: torrentBackend.currentCategory === modelData.id
            onClicked: torrentBackend.currentCategory = modelData.id
        }
    }

    Item { Layout.preferredHeight: 6 }
    CachyDivider { Layout.fillWidth: true }
    Item { Layout.preferredHeight: 4 }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 10
        Layout.rightMargin: 6

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: "Motores (" + torrentBackend.enabledEnginesCount + ")"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: CachyTheme.textMuted
            elide: Text.ElideRight
        }

        Text {
            text: torrentBackend.resultsModel.engineFilter !== "all" ? "Ver todos" : "Configurar"
            font.pixelSize: 11
            font.weight: Font.Medium
            color: CachyTheme.accentColor

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler {
                onTapped: {
                    if (torrentBackend.resultsModel.engineFilter !== "all") {
                        torrentBackend.resultsModel.engineFilter = "all";
                    } else {
                        root.toggleEnginesInspector();
                    }
                }
            }
        }
    }

    CachySidebarItem {
        Layout.fillWidth: true
        text: "Todos los motores"
        iconGlyph: "🌐"
        iconColor: "#0A84FF"
        badgeText: torrentBackend.totalResultsCount > 0 ? torrentBackend.totalResultsCount.toString() : ""
        selected: torrentBackend.resultsModel.engineFilter === "all"
        onClicked: torrentBackend.resultsModel.engineFilter = "all"
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 2

            Repeater {
                model: torrentBackend.engines

                CachySidebarItem {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name
                    iconGlyph: modelData.running ? "◌" : (modelData.enabled ? "●" : "○")
                    iconColor: modelData.running
                        ? "#0A84FF"
                        : (modelData.enabled ? "#30D158" : CachyTheme.textMuted)
                    badgeText: modelData.resultCount > 0 ? modelData.resultCount.toString() : ""
                    selected: torrentBackend.resultsModel.engineFilter === modelData.id
                    onClicked: {
                        if (torrentBackend.resultsModel.engineFilter === modelData.id) {
                            torrentBackend.resultsModel.engineFilter = "all";
                        } else {
                            torrentBackend.resultsModel.engineFilter = modelData.id;
                        }
                    }
                }
            }
        }
    }
}
