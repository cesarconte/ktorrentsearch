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
        onActivated: queryInput.forceActiveFocus()
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
        ColumnLayout {
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
                                root.showEnginesInspector = !root.showEnginesInspector;
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

            // Quick toggle: only seeded torrents (seeds >= 1)
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

            // -----------------------------------------------------------------
            // CENTER PANE: Search Command Bar + Sorting Bar + Results ListView
            // -----------------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 14
                spacing: 10

                // A. Primary Search Command Card
                CachyCard {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    topPadding: 10
                    bottomPadding: 10
                    leftPadding: 12
                    rightPadding: 12

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
                                                if (isCurrentSort) {
                                                    torrentBackend.resultsModel.sortDescending = !torrentBackend.resultsModel.sortDescending;
                                                } else {
                                                    torrentBackend.resultsModel.sortField = modelData.key;
                                                    torrentBackend.resultsModel.sortDescending = (modelData.key !== "name");
                                                }
                                            }
                                        }

                                        Text {
                                            id: sortChipText
                                            anchors.centerIn: parent
                                            text: modelData.label + (parent.isCurrentSort ? (torrentBackend.resultsModel.sortDescending ? " ↓" : " ↑") : "")
                                            font.pixelSize: 11
                                            font.weight: parent.isCurrentSort ? Font.Bold : Font.Medium
                                            color: parent.isCurrentSort ? CachyTheme.accentColor : CachyTheme.textSecondary
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // B. Empty State / Quick Welcome Card (when no results yet)
                Item {
                    visible: torrentBackend.totalResultsCount === 0 && !torrentBackend.isSearching
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    CachyCard {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 32, 540)

                        contentItem: ColumnLayout {
                            spacing: 14

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 52
                                height: 52
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
                                            required property string modelData
                                            height: 26
                                            implicitWidth: recentLabel.implicitWidth + 18
                                            radius: 13
                                            color: CachyTheme.surfacePill
                                            border.width: 1
                                            border.color: CachyTheme.specularBorder

                                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                                            TapHandler {
                                                onTapped: {
                                                    queryInput.text = modelData;
                                                    torrentBackend.search(modelData);
                                                }
                                            }

                                            Text {
                                                id: recentLabel
                                                anchors.centerIn: parent
                                                text: "🕒 " + parent.modelData
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

                // C. Virtualized Torrent Results ListView (Zero Overflow Policy)
                ListView {
                    id: resultsListView
                    visible: torrentBackend.totalResultsCount > 0 || torrentBackend.isSearching
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    boundsBehavior: Flickable.StopAtBounds
                    model: torrentBackend.resultsModel

                    QQC2.ScrollBar.vertical: QQC2.ScrollBar {
                        policy: QQC2.ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        id: rowCard
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

                        width: resultsListView.width - 10
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
                                torrentBackend.downloadByLink(rowCard.link, rowCard.engineId, rowCard.name);
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
                                onTriggered: torrentBackend.downloadByLink(rowCard.link, rowCard.engineId, rowCard.name)
                            }

                            QQC2.MenuItem {
                                icon.name: "edit-copy"
                                text: rowCard.isMagnet ? "Copiar enlace Magnet" : "Copiar enlace del torrent"
                                onTriggered: torrentBackend.copyMagnetOrLink(rowCard.link)
                            }

                            QQC2.MenuItem {
                                enabled: rowCard.descLink.length > 0
                                icon.name: "internet-web-browser"
                                text: "Abrir página de descripción en el navegador"
                                onTriggered: torrentBackend.openDescriptionPage(rowCard.descLink)
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
                                color: rowCard.isMagnet
                                    ? Qt.rgba(0.04, 0.52, 1.0, 0.16)
                                    : Qt.rgba(0.75, 0.35, 0.95, 0.16)
                                border.width: 1
                                border.color: rowCard.isMagnet
                                    ? Qt.rgba(0.04, 0.52, 1.0, 0.35)
                                    : Qt.rgba(0.75, 0.35, 0.95, 0.35)

                                Text {
                                    anchors.centerIn: parent
                                    text: rowCard.isMagnet ? "🧲" : "📦"
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
                                    text: rowCard.name
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
                                        height: 20
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
                                            text: rowCard.engineName
                                            font.pixelSize: 11
                                            font.weight: Font.Medium
                                            color: CachyTheme.accentColor
                                            elide: Text.ElideRight
                                        }
                                    }

                                    // Size Badge
                                    Text {
                                        text: "💾 " + rowCard.formattedSize
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: CachyTheme.textSecondary
                                    }

                                    // Date Badge
                                    Text {
                                        visible: rowCard.formattedDate !== "—"
                                        text: "📅 " + rowCard.formattedDate
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
                                    height: 26
                                    implicitWidth: Math.max(58, seedsText.implicitWidth + 16)
                                    radius: 8
                                    color: rowCard.seeds > 0
                                        ? Qt.rgba(0.19, 0.82, 0.35, 0.16)
                                        : Qt.rgba(0.56, 0.56, 0.58, 0.14)
                                    border.width: 1
                                    border.color: rowCard.seeds > 0
                                        ? Qt.rgba(0.19, 0.82, 0.35, 0.42)
                                        : CachyTheme.specularBorder

                                    Text {
                                        id: seedsText
                                        anchors.centerIn: parent
                                        text: "▲ " + rowCard.seeds
                                        font.pixelSize: 12
                                        font.weight: Font.Bold
                                        color: rowCard.seeds > 0 ? "#30D158" : CachyTheme.textMuted
                                    }
                                }

                                // Leechers Pill
                                Rectangle {
                                    height: 26
                                    implicitWidth: Math.max(52, leechText.implicitWidth + 16)
                                    radius: 8
                                    color: Qt.rgba(0.04, 0.52, 1.0, 0.12)
                                    border.width: 1
                                    border.color: Qt.rgba(0.04, 0.52, 1.0, 0.28)

                                    Text {
                                        id: leechText
                                        anchors.centerIn: parent
                                        text: "▼ " + rowCard.leechers
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
                                    tooltipText: rowCard.isMagnet ? "Copiar enlace Magnet al portapapeles" : "Copiar URL del torrent"
                                    onClicked: torrentBackend.copyMagnetOrLink(rowCard.link)
                                }

                                CachyToolbarButton {
                                    visible: rowCard.descLink.length > 0
                                    text: "🌐"
                                    tooltipText: "Abrir página del torrent en el navegador"
                                    onClicked: torrentBackend.openDescriptionPage(rowCard.descLink)
                                }

                                CachyButton {
                                    primary: true
                                    text: "⬇ Descargar"
                                    onClicked: torrentBackend.downloadByLink(rowCard.link, rowCard.engineId, rowCard.name)
                                }
                            }
                        }
                    }
                }
            }

            // 1px Hard Vertical Divider before Right Engines Inspector
            Rectangle {
                visible: root.showEnginesInspector
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: CachyTheme.hardDividerColor
            }

            // -----------------------------------------------------------------
            // RIGHT PANE: Search Engines & Nova3 Plugins Inspector
            // -----------------------------------------------------------------
            ColumnLayout {
                visible: root.showEnginesInspector
                Layout.preferredWidth: 310
                Layout.minimumWidth: 290
                Layout.maximumWidth: 330
                Layout.fillHeight: true
                Layout.margins: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: "Motores Nova3"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        color: CachyTheme.textPrimary
                        elide: Text.ElideRight
                    }

                    CachyToolbarButton {
                        text: "✕"
                        tooltipText: "Cerrar panel de motores"
                        onClicked: root.showEnginesInspector = false
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    wrapMode: Text.WordWrap
                    text: "Motores autónomos compatibles con la especificación nova3 de qBittorrent. Siguen funcionando aunque desinstales qBittorrent."
                    font.pixelSize: 12
                    color: CachyTheme.textSecondary
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    CachyButton {
                        Layout.fillWidth: true
                        text: "Activar todos"
                        onClicked: torrentBackend.setAllEnginesEnabled(true)
                    }

                    CachyButton {
                        Layout.fillWidth: true
                        text: "Ninguno"
                        onClicked: torrentBackend.setAllEnginesEnabled(false)
                    }
                }

                CachyButton {
                    Layout.fillWidth: true
                    enabled: !torrentBackend.updatingEngines
                    text: torrentBackend.updatingEngines ? "Comprobando actualizaciones…" : "⟳ Buscar actualizaciones"
                    onClicked: torrentBackend.updateEngines()
                }

                QQC2.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: torrentBackend.engines

                            CachyCard {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                topPadding: 10
                                bottomPadding: 10
                                leftPadding: 12
                                rightPadding: 12

                                contentItem: RowLayout {
                                    spacing: 10

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 2

                                        Text {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            text: modelData.name
                                            font.pixelSize: 13
                                            font.weight: Font.Bold
                                            color: CachyTheme.textPrimary
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            text: modelData.url
                                            font.pixelSize: 11
                                            color: CachyTheme.textMuted
                                            elide: Text.ElideMiddle
                                        }
                                    }

                                    CachySwitch {
                                        checked: modelData.enabled
                                        onToggled: torrentBackend.setEngineEnabled(modelData.id, checked)
                                    }
                                }
                            }
                        }
                    }
                }

                CachyDivider { Layout.fillWidth: true }

                CachyButton {
                    Layout.fillWidth: true
                    primary: true
                    text: "+ Instalar plugin (.py)…"
                    onClicked: pluginFileDialog.open()
                }
            }
        }
    ]

    // =========================================================================
    // 4. FLOATING GOLDEN GATE TOAST OVERLAY
    // =========================================================================
    overlay: [
        Rectangle {
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
    ]
}
