/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "torrentmodel.h"
#include "torrentsearchbackend.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ktorrentsearch"));
    app.setApplicationDisplayName(QStringLiteral("KTorrentSearch — Buscador de Torrents"));
    app.setOrganizationName(QStringLiteral("Cesar"));
    app.setOrganizationDomain(QStringLiteral("cesar.org"));
    app.setDesktopFileName(QStringLiteral("org.cesar.ktorrentsearch"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("ktorrent"), QIcon::fromTheme(QStringLiteral("transmission"))));

    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Buscador nativo de Torrents en CachyOS UI con integración directa en Transmission Qt"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("query"), QStringLiteral("Término de búsqueda inicial opcional"));
    parser.process(app);

    TorrentSearchBackend backend;

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("/home/cesar/.local/lib/qt6/qml"));
    engine.rootContext()->setContextProperty(QStringLiteral("torrentBackend"), &backend);

    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    const QStringList posArgs = parser.positionalArguments();
    if (!posArgs.isEmpty()) {
        const QString initialQuery = posArgs.join(QLatin1Char(' ')).trimmed();
        if (!initialQuery.isEmpty()) {
            QTimer::singleShot(150, &backend, [&backend, initialQuery]() {
                backend.search(initialQuery);
            });
        }
    }

    return app.exec();
}
