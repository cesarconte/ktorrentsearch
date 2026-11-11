/*
 * SPDX-FileCopyrightText: 2026 Cesar <yugurta@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "torrentmodel.h"
#include "torrentsearchbackend.h"

#include <QTest>

class TestTorrentSearch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testParseNovaLineMagnet()
    {
        const QString line = QStringLiteral(
            "magnet:?xt=urn:btih:2C6B6858D61DA9543D4231A71DB4B1C9264B0685&dn=Ubuntu+22.04+LTS"
            "|Ubuntu%2022.04%20LTS%20%28x86_64%29|3654957056|34|11|https://thepiratebay.org"
            "|https://thepiratebay.org/description.php?id=59191690|1652877231");

        auto itemOpt = TorrentListModel::parseNovaLine(line, QStringLiteral("piratebay"), QStringLiteral("The Pirate Bay"));
        QVERIFY(itemOpt.has_value());
        QCOMPARE(itemOpt->name, QStringLiteral("Ubuntu 22.04 LTS (x86_64)"));
        QCOMPARE(itemOpt->sizeBytes, 3654957056LL);
        QCOMPARE(itemOpt->formattedSize, QStringLiteral("3.40 GB"));
        QCOMPARE(itemOpt->seeds, 34);
        QCOMPARE(itemOpt->leechers, 11);
        QCOMPARE(itemOpt->engineId, QStringLiteral("piratebay"));
        QCOMPARE(itemOpt->engineName, QStringLiteral("The Pirate Bay"));
        QVERIFY(itemOpt->isMagnet);
        QCOMPARE(itemOpt->dedupKey, QStringLiteral("btih:2c6b6858d61da9543d4231a71db4b1c9264b0685"));
    }

    void testParseNovaLineInvalid()
    {
        QVERIFY(!TorrentListModel::parseNovaLine(QString()).has_value());
        QVERIFY(!TorrentListModel::parseNovaLine(QStringLiteral("invalid line without pipes")).has_value());
        QVERIFY(!TorrentListModel::parseNovaLine(QStringLiteral("only|three|fields")).has_value());
    }

    void testDeduplicationAndMerge()
    {
        TorrentListModel model;

        auto item1 = TorrentListModel::parseNovaLine(
            QStringLiteral("magnet:?xt=urn:btih:AABBCCDDEEFF00112233445566778899AABBCCDD|CachyOS%20ISO|2147483648|25|5|https://thepiratebay.org||1700000000"),
            QStringLiteral("piratebay"),
            QStringLiteral("The Pirate Bay"));
        auto item2 = TorrentListModel::parseNovaLine(
            QStringLiteral("magnet:?xt=urn:btih:aabbccddeeff00112233445566778899aabbccdd|CachyOS%20ISO|2147483648|80|12|https://torrents-csv.com|https://torrents-csv.com/1|1700000000"),
            QStringLiteral("torrentscsv"),
            QStringLiteral("torrents-csv"));

        QVERIFY(item1.has_value());
        QVERIFY(item2.has_value());

        QVERIFY(model.addOrMergeItem(*item1));
        QCOMPARE(model.rowCount(), 1);

        // Second item has identical btih hash -> should merge rather than duplicate
        QVERIFY(!model.addOrMergeItem(*item2));
        QCOMPARE(model.rowCount(), 1);

        const TorrentItem *merged = model.itemAt(0);
        QVERIFY(merged != nullptr);
        QCOMPARE(merged->seeds, 80);
        QCOMPARE(merged->leechers, 12);
        QVERIFY(merged->engineName.contains(QStringLiteral("The Pirate Bay")));
        QVERIFY(merged->engineName.contains(QStringLiteral("torrents-csv")));
        QCOMPARE(merged->descLink, QStringLiteral("https://torrents-csv.com/1"));
    }

    void testFilterAndSortProxy()
    {
        TorrentListModel source;
        TorrentFilterProxyModel proxy;
        proxy.setSourceModel(&source);

        auto t1 = TorrentListModel::parseNovaLine(
            QStringLiteral("magnet:?xt=urn:btih:1111111111111111111111111111111111111111|Ubuntu%2024.04%20Desktop|4000000000|10|2|https://e1.org||1700000100"),
            QStringLiteral("piratebay"),
            QStringLiteral("The Pirate Bay"));
        auto t2 = TorrentListModel::parseNovaLine(
            QStringLiteral("magnet:?xt=urn:btih:2222222222222222222222222222222222222222|CachyOS%20Desktop%20KDE|3000000000|150|20|https://e2.org||1700000500"),
            QStringLiteral("yts"),
            QStringLiteral("YTS"));
        auto t3 = TorrentListModel::parseNovaLine(
            QStringLiteral("magnet:?xt=urn:btih:3333333333333333333333333333333333333333|Dead%20Torrent%20ISO|1000000000|0|0|https://e1.org||1600000000"),
            QStringLiteral("piratebay"),
            QStringLiteral("The Pirate Bay"));

        source.addOrMergeItem(*t1);
        source.addOrMergeItem(*t2);
        source.addOrMergeItem(*t3);

        QCOMPARE(proxy.rowCount(), 3);

        // Default sort is seeds descending -> t2 (150), t1 (10), t3 (0)
        QCOMPARE(proxy.getRow(0).value(QStringLiteral("seeds")).toInt(), 150);
        QCOMPARE(proxy.getRow(1).value(QStringLiteral("seeds")).toInt(), 10);
        QCOMPARE(proxy.getRow(2).value(QStringLiteral("seeds")).toInt(), 0);

        // Filter minSeeds >= 1
        proxy.setMinSeeds(1);
        QCOMPARE(proxy.rowCount(), 2);

        // Filter by multi-word text "cachyos kde"
        proxy.setFilterText(QStringLiteral("cachyos kde"));
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.getRow(0).value(QStringLiteral("name")).toString(), QStringLiteral("CachyOS Desktop KDE"));

        // Reset text filter and sort by size descending
        proxy.setFilterText(QString());
        proxy.setSortField(QStringLiteral("size"));
        proxy.setSortDescending(true);
        QCOMPARE(proxy.getRow(0).value(QStringLiteral("name")).toString(), QStringLiteral("Ubuntu 24.04 Desktop"));
    }

    void testParseCapabilitiesXml()
    {
        const QString xml = QStringLiteral(
            "<capabilities>\n"
            "  <piratebay>\n"
            "    <name>The Pirate Bay</name>\n"
            "    <url>https://thepiratebay.org</url>\n"
            "    <categories>games movies music software</categories>\n"
            "  </piratebay>\n"
            "  <torrentscsv>\n"
            "    <name>torrents-csv</name>\n"
            "    <url>https://torrents-csv.com</url>\n"
            "    <categories />\n"
            "  </torrentscsv>\n"
            "</capabilities>");

        const QVector<SearchEngineInfo> engines = TorrentSearchBackend::parseCapabilitiesXml(xml);
        QCOMPARE(engines.size(), 2);
        QCOMPARE(engines[0].id, QStringLiteral("piratebay"));
        QCOMPARE(engines[0].name, QStringLiteral("The Pirate Bay"));
        QCOMPARE(engines[0].categories.size(), 4);
        QCOMPARE(engines[1].id, QStringLiteral("torrentscsv"));
        QVERIFY(engines[1].categories.isEmpty());
    }

    void testPluginVersionParsing()
    {
        QCOMPARE(TorrentSearchBackend::versionFromHeader(QByteArrayLiteral("# VERSION: 4.15\n# AUTHORS: x\n")), QStringLiteral("4.15"));
        QCOMPARE(TorrentSearchBackend::versionFromHeader(QByteArrayLiteral("#VERSION:1.3.2\r\nimport re")), QStringLiteral("1.3.2"));
        QVERIFY(TorrentSearchBackend::versionFromHeader(QByteArrayLiteral("<html>404</html>\n")).isEmpty());
        QVERIFY(TorrentSearchBackend::versionFromHeader(QByteArray()).isEmpty());
    }

    void testVersionComparison()
    {
        const auto cmp = [](const char16_t *x, const char16_t *y) {
            return TorrentSearchBackend::compareVersions(QString(x), QString(y));
        };
        QVERIFY(cmp(u"1.10", u"1.9") > 0); // numeric, not lexicographic
        QVERIFY(cmp(u"1.3", u"1.4") < 0);
        QCOMPARE(cmp(u"2.0", u"2"), 0);
        QVERIFY(TorrentSearchBackend::compareVersions(QStringLiteral("1.0"), QString()) > 0);
    }

    void testUpstreamPluginUrl()
    {
        QVERIFY(TorrentSearchBackend::upstreamPluginUrl(QStringLiteral("piratebay")).toString()
                    .contains(QStringLiteral("qbittorrent/search-plugins/master/nova3/engines/piratebay.py")));
        QVERIFY(TorrentSearchBackend::upstreamPluginUrl(QStringLiteral("therarbg")).toString()
                    .contains(QStringLiteral("BurningMop")));
    }

    void testBackendStandaloneRuntime()
    {
        TorrentSearchBackend backend;
        QVERIFY(!backend.novaRuntimePath().isEmpty());
        QVERIFY(backend.engines().size() >= 10);
    }
};

QTEST_MAIN(TestTorrentSearch)
#include "test_torrentsearch.moc"
