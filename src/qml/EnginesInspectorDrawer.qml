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

    signal closeRequested()
    signal openPluginDialog()

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
            onClicked: root.closeRequested()
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
                    id: engineCard
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
                                text: engineCard.modelData.name
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: CachyTheme.textPrimary
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: engineCard.modelData.url
                                font.pixelSize: 11
                                color: CachyTheme.textMuted
                                elide: Text.ElideMiddle
                            }
                        }

                        CachySwitch {
                            checked: engineCard.modelData.enabled
                            onToggled: torrentBackend.setEngineEnabled(engineCard.modelData.id, checked)
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
        onClicked: root.openPluginDialog()
    }
}
