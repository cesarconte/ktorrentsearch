/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Dialogs as QtDialogs
import CachyOS.UI

CachyWindow {
    id: root

    title: "KTorrentSearch — Buscador de Torrents"
    width: 1020
    height: 680
    minimumWidth: 800
    minimumHeight: 500

    showTrafficLights: true
    showTitleBarDivider: true
    showWindowFrame: true
    sidebarVisible: true
    sidebarWidth: 218
    cornerRadius: CachyTheme.radiusWindow

    property bool showEnginesInspector: false
    property string toastText: ""
    property bool toastError: false

    readonly property var categoryList: [
        { id: "all",      label: "Todo",       glyph: "❖", color: "#007AFF" },
        { id: "movies",   label: "Películas",  glyph: "🎬", color: "#FF453A" },
        { id: "tv",       label: "Series TV",  glyph: "📺", color: "#BF5AF2" },
        { id: "music",    label: "Música",     glyph: "🎵", color: "#FF2D55" },
        { id: "games",    label: "Juegos",     glyph: "🎮", color: "#30D158" },
        { id: "software", label: "Software",   glyph: "💿", color: "#5AC8FA" },
        { id: "anime",    label: "Anime",      glyph: "✦", color: "#FF9F0A" },
        { id: "books",    label: "Libros",     glyph: "📚", color: "#FFD60A" }
    ]

    function currentCategoryTitle() {
        for (let i = 0; i < categoryList.length; ++i) {
            if (categoryList[i].id === torrentBackend.currentCategory) {
                return categoryList[i].label;
            }
        }
        return "Todo";
    }

    QtDialogs.FileDialog {
        id: pluginFileDialog
        title: "Instalar plugin de búsqueda nova3 (.py)"
        fileMode: QtDialogs.FileDialog.OpenFile
        nameFilters: ["Plugins Python de qBittorrent (*.py)"]
        onAccepted: {
            torrentBackend.installPluginFromFile(selectedFile.toString());
        }
    }

    Timer {
        id: toastTimer
        interval: 3400
        repeat: false
        onTriggered: root.toastText = ""
    }

    Connections {
        target: torrentBackend
        function onToastNotification(message, isError) {
            root.toastText = message;
            root.toastError = isError;
            toastTimer.restart();
        }
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: searchBar.focusSearch()
    }
    Shortcut {
        sequence: "Ctrl+L"
        onActivated: inlineFilterField.forceActiveFocus()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (torrentBackend.isSearching) {
                torrentBackend.stopSearch();
            } else if (root.showEnginesInspector) {
                root.showEnginesInspector = false;
            }
        }
    }

    // =========================================================================
    // 1. LEFT EDGE-TO-EDGE GOLDEN GATE SIDEBAR
    // =========================================================================
    sidebarHeaderActions: [
        Row {
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter

            CachyToolbarButton {
                text: "⚡"
                tooltipText: "Abrir Transmission Qt"
                onClicked: torrentBackend.openTransmission()
            }

            CachyToolbarButton {
                text: torrentBackend.updatingEngines ? "…" : "⟳"
                enabled: !torrentBackend.updatingEngines
                tooltipText: "Buscar actualizaciones de los motores de búsqueda"
                onClicked: torrentBackend.updateEngines()
            }

            CachyToolbarButton {
                text: "⚙"
                checkable: true
                checked: root.showEnginesInspector
                tooltipText: "Gestionar motores y plugins de búsqueda (.py)"
                onClicked: root.showEnginesInspector = !root.showEnginesInspector
            }
        }
    ]

    sidebar: [
        SidebarView {
            categoryList: root.categoryList
            showEnginesInspector: root.showEnginesInspector
            onToggleEnginesInspector: root.showEnginesInspector = !root.showEnginesInspector
        }
    ]

    // =========================================================================
    // 2. 44PX GOLDEN GATE HEADER TOOLBAR
    // =========================================================================
    headerLeading: [
        RowLayout {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Text {
                text: root.currentCategoryTitle()
                font.pixelSize: 15
                font.weight: Font.Bold
                color: CachyTheme.textPrimary
            }

            Rectangle {
                visible: torrentBackend.totalResultsCount > 0
                height: 22
                implicitWidth: countBadgeLabel.implicitWidth + 16
                radius: 11
                color: CachyTheme.surfacePill
                border.width: 1
                border.color: CachyTheme.specularBorder

                Text {
                    id: countBadgeLabel
                    anchors.centerIn: parent
                    text: torrentBackend.filteredResultsCount === torrentBackend.totalResultsCount
                        ? torrentBackend.totalResultsCount + " torrents"
                        : torrentBackend.filteredResultsCount + " de " + torrentBackend.totalResultsCount
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: CachyTheme.textSecondary
                }
            }
        }
    ]

    headerCenter: [
        Item { width: 1; height: 1 }
    ]

    headerActions: [
        RowLayout {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            CachyToolbarButton {
                iconName: "view-filter"
                text: torrentBackend.resultsModel.minSeeds > 0 ? "▲ Con semillas (ON)" : "▲ Con semillas"
                checkable: true
                checked: torrentBackend.resultsModel.minSeeds > 0
                tooltipText: "Ocultar torrents muertos sin semillas (seeds = 0)"
                onClicked: {
                    torrentBackend.resultsModel.minSeeds = torrentBackend.resultsModel.minSeeds > 0 ? 0 : 1;
                }
            }

            // Secondary in-memory filter pill
            Rectangle {
                Layout.preferredWidth: 168
                height: 28
                radius: 14
                color: CachyTheme.surfacePill
                border.width: 1
                border.color: inlineFilterField.activeFocus ? CachyTheme.accentColor : CachyTheme.specularBorder

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 6

                    Text {
                        text: "⚡"
                        font.pixelSize: 11
                        color: CachyTheme.textMuted
                    }

                    QQC2.TextField {
                        id: inlineFilterField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        placeholderText: "Filtrar lista…"
                        placeholderTextColor: CachyTheme.textMuted
                        color: CachyTheme.textPrimary
                        font.pixelSize: 12
                        background: null
                        text: torrentBackend.resultsModel.filterText
                        onTextChanged: torrentBackend.resultsModel.filterText = text
                    }

                    Text {
                        visible: inlineFilterField.text.length > 0
                        text: "✕"
                        font.pixelSize: 10
                        font.bold: true
                        color: CachyTheme.textSecondary
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: inlineFilterField.clear() }
                    }
                }
            }

            CachyToolbarButton {
                iconName: "preferences-system"
                text: "⚙ Motores"
                checkable: true
                checked: root.showEnginesInspector
                tooltipText: "Activar/desactivar motores o añadir nuevos plugins .py"
                onClicked: root.showEnginesInspector = !root.showEnginesInspector
            }
        }
    ]

    // =========================================================================
    // 3. MAIN BODY (Center Search + Virtualized Results + Right Engines Drawer)
    // =========================================================================
    contentData: [
        RowLayout {
            anchors.fill: parent
            spacing: 0

            // CENTER PANE
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 14
                spacing: 10

                // A. Search Bar Card
                SearchBarCard {
                    id: searchBar
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }

                // B. Empty State / Quick Welcome Card
                WelcomeView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onRecentSearchSelected: function(query) {
                        searchBar.setQueryText(query);
                        torrentBackend.search(query);
                    }
                }

                // C. Virtualized Torrent Results ListView
                ResultsListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            // 1px Vertical Divider
            Rectangle {
                visible: root.showEnginesInspector
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: CachyTheme.hardDividerColor
            }

            // RIGHT PANE: Engines Inspector
            EnginesInspectorDrawer {
                visible: root.showEnginesInspector
                onCloseRequested: root.showEnginesInspector = false
                onOpenPluginDialog: pluginFileDialog.open()
            }
        }
    ]

    // =========================================================================
    // 4. FLOATING GOLDEN GATE TOAST OVERLAY
    // =========================================================================
    overlay: [
        ToastOverlay {
            toastText: root.toastText
            toastError: root.toastError
        }
    ]
}
