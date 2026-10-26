// no-port-check: NereusSDR-original tests for the Thetis web image settings.
// Web image item: the known-host lock, the BSDWorld URL fix, file:// URLs
// read with no network request, "No Image", aspect-kept drawing, the
// interval for old records and new items, the 4-char ID, the fetch state
// and inert previews. Network fetches use a loopback server only.
// Modification history (NereusSDR):
//   2026-10-26: first version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
#include <QtTest>
#include <QBuffer>
#include <QImage>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QPainter>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrl>
#include <memory>

#include "core/mmio/FourChar.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/WebImageItem.h"

using namespace NereusSDR;

namespace {

// tst_mmio_four_char's guid: fourChar("webimage", 0, guid) is "web4".
const QUuid kGuid(QStringLiteral("3f2504e0-4f89-11d3-9a0c-0305e82c3301"));
const QColor kRed(255, 0, 0);
const QColor kFallback(0x20, 0x20, 0x20);

QString writePng(const QTemporaryDir& dir, const QString& name, int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(kRed);
    const QString path = dir.filePath(name);
    if (!img.save(path, "PNG")) { return QString(); }
    return QUrl::fromLocalFile(path).toString();
}

QByteArray pngBytes()
{
    QImage img(8, 4, QImage::Format_ARGB32);
    img.fill(kRed);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return bytes;
}

QImage render(WebImageItem& item, int w, int h)
{
    QImage out(w, h, QImage::Format_ARGB32);
    out.fill(Qt::black);
    QPainter p(&out);
    item.paint(p, w, h);
    p.end();
    return out;
}

// A loopback HTTP server: answers every request with status and body,
// and remembers each request line.
class LoopbackServer : public QTcpServer {
public:
    LoopbackServer(int status, QByteArray body) : m_status(status), m_body(std::move(body))
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    const QByteArray request = socket->readAll();
                    requestLines.append(QString::fromLatin1(request.left(request.indexOf('\r'))));
                    const QByteArray reason = m_status == 200 ? "OK" : "Not Found";
                    socket->write("HTTP/1.1 " + QByteArray::number(m_status) + ' ' + reason
                                  + "\r\nContent-Type: image/png\r\nContent-Length: "
                                  + QByteArray::number(m_body.size())
                                  + "\r\nConnection: close\r\n\r\n" + m_body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    QStringList requestLines;

private:
    int m_status;
    QByteArray m_body;
};

} // namespace

class TestWebImageItem : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("qt.qpa.fonts.warning=false"));
    }

    void thetisDefaults()
    {
        QCOMPARE(kWebImageThetisDefaults.intervalSecs, 120);
        QCOMPARE(kWebImageThetisDefaults.backgroundIntervalSecs, 5);
        QCOMPARE(kWebImageThetisDefaults.widthScale, 1.0);
        QCOMPARE(kWebImageThetisMinIntervalSecs, 30);
        QCOMPARE(kWebImageMaxIntervalSecs, 7200);
        QCOMPARE(kWebImageMinIntervalSecs, 1);
    }

    void knownHostForcesIntervalAndBypassOff_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<bool>("locked");
        QTest::newRow("hamqsl") << QStringLiteral("https://www.HamQSL.com/solar.php") << true;
        QTest::newRow("bsdworld") << QStringLiteral("https://bsdworld.org/flux-light.svgz") << true;
        QTest::newRow("kc2g") << QStringLiteral("https://prop.kc2g.com/renders/current/mufd-normal-now.svg") << true;
        QTest::newRow("nasa") << QStringLiteral("https://soho.nascom.nasa.gov/data/realtime/c2/512/latest.jpg") << false;
        QTest::newRow("noaa") << QStringLiteral("https://services.swpc.noaa.gov/images/station-k-index.png") << false;
    }

    void knownHostForcesIntervalAndBypassOff()
    {
        QFETCH(QString, url);
        QFETCH(bool, locked);
        WebImageItem item;
        item.setFetchEnabled(false);
        item.setRefreshInterval(45);
        item.setBypassCache(true);
        item.setUrl(url);
        QCOMPARE(WebImageItem::isLockedHost(url), locked);
        QCOMPARE(item.refreshInterval(), locked ? 600 : 45);
        QCOMPARE(item.bypassCache(), !locked);
        // While locked, neither can be changed.
        item.setRefreshInterval(60);
        item.setBypassCache(true);
        QCOMPARE(item.refreshInterval(), locked ? 600 : 60);
        QCOMPARE(item.bypassCache(), !locked);
        // A saved record keeps the lock.
        WebImageItem copy;
        copy.setFetchEnabled(false);
        QVERIFY(copy.deserialize(item.serialize()));
        QCOMPARE(copy.refreshInterval(), item.refreshInterval());
        QCOMPARE(copy.bypassCache(), item.bypassCache());
        QCOMPARE(item.networkRequestCount(), 0);
        QCOMPARE(copy.networkRequestCount(), 0);
    }

    void bsdWorldUrlsGetLightOrDark()
    {
        QCOMPARE(WebImageItem::fixBsdWorldUrl(QStringLiteral("https://bsdworld.org/flux.svgz"), true),
                 QStringLiteral("https://bsdworld.org/flux-dark.svgz"));
        QCOMPARE(WebImageItem::fixBsdWorldUrl(
                     QStringLiteral("https://bsdworld.org/DXCC/cqzone/14/latest.webp"), false),
                 QStringLiteral("https://bsdworld.org/DXCC/cqzone/14/latest-light.webp"));
        // Already has a mode, not a preset, or another host: nothing to fix.
        QVERIFY(WebImageItem::fixBsdWorldUrl(QStringLiteral("https://bsdworld.org/flux-dark.svgz"), false).isEmpty());
        QVERIFY(WebImageItem::fixBsdWorldUrl(QStringLiteral("https://bsdworld.org/other.svgz"), false).isEmpty());
        QVERIFY(WebImageItem::fixBsdWorldUrl(QStringLiteral("https://www.hamqsl.com/solar.php"), false).isEmpty());
        QCOMPARE(WebImageItem::resolveLightMode(QStringLiteral("https://bsdworld.org/ssn<light_mode>.svgz"), true),
                 QStringLiteral("https://bsdworld.org/ssn-dark.svgz"));
    }

    void presetsFollowThetis()
    {
        const auto groups = WebImageItem::presetGroups();
        QCOMPARE(groups.size(), 4);
        QCOMPARE(groups[0].name, QStringLiteral("HamQSL"));
        QCOMPARE(groups[0].presets.size(), 23);
        QCOMPARE(groups[1].name, QStringLiteral("BSDWorld"));
        QCOMPARE(groups[1].presets.size(), 25);
        QCOMPARE(groups[2].name, QStringLiteral("NASA"));
        QCOMPARE(groups[2].presets.size(), 8);
        QCOMPARE(groups[3].name, QStringLiteral("NOAA"));
        QCOMPARE(groups[3].presets.size(), 7);
        QCOMPARE(groups[0].presets[3].url, QStringLiteral("https://www.hamqsl.com/solar.php"));
    }

    void fileUrlLoadsWithNoNetworkRequest()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString url = writePng(dir, QStringLiteral("image.png"), 30, 20);
        QVERIFY(url.startsWith(QStringLiteral("file://")));
        WebImageItem item;
        QSignalSpy states(&item, &WebImageItem::stateChanged);
        item.setUrl(url);
        QVERIFY(!item.currentImage().isNull());
        QCOMPARE(item.currentImage().size(), QSize(30, 20));
        QCOMPARE(item.networkRequestCount(), 0);
        QCOMPARE(item.state(), WebImageItem::State::Ok);
        QCOMPARE(item.stateText(), QStringLiteral("ok"));
        QVERIFY(states.size() >= 1);

        WebImageItem missing;
        missing.setUrl(QUrl::fromLocalFile(dir.filePath(QStringLiteral("none.png"))).toString());
        QVERIFY(missing.currentImage().isNull());
        QCOMPARE(missing.networkRequestCount(), 0);
        QCOMPARE(missing.stateText(), QStringLiteral("url issue"));
    }

    void noImagePaintsNoImage()
    {
        WebImageItem item;
        item.setFetchEnabled(false);
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        QCOMPARE(item.stateText(), QStringLiteral("idle"));
        const QImage out = render(item, 200, 100);
        int textPixels = 0;
        int fallbackPixels = 0;
        for (int y = 0; y < out.height(); ++y) {
            for (int x = 0; x < out.width(); ++x) {
                const QColor c = out.pixelColor(x, y);
                if (c.red() > 0x60 && c.red() == c.green() && c.green() == c.blue()) { ++textPixels; }
                if (c == kFallback) { ++fallbackPixels; }
            }
        }
        QVERIFY2(textPixels > 20, qPrintable(QString::number(textPixels)));
        QVERIFY(fallbackPixels > out.width() * out.height() / 2);
        // The corners stay the fallback colour; the text is in the middle.
        QCOMPARE(out.pixelColor(2, 2), kFallback);
        QCOMPARE(out.pixelColor(197, 97), kFallback);
    }

    void drawingKeepsAspectRatio()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        WebImageItem item;
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        item.setUrl(writePng(dir, QStringLiteral("wide.png"), 100, 50));
        QVERIFY(!item.currentImage().isNull());

        // 400 x 300: the image fills the width and is 200 high.
        QImage out = render(item, 400, 300);
        QCOMPARE(out.pixelColor(200, 100), kRed);
        QCOMPARE(out.pixelColor(200, 190), kRed);
        QCOMPARE(out.pixelColor(200, 250), kFallback);

        // Width scale 0.5: 200 wide, centred, 100 high.
        item.setWidthScale(0.5);
        out = render(item, 400, 300);
        QCOMPARE(out.pixelColor(50, 50), kFallback);
        QCOMPARE(out.pixelColor(200, 50), kRed);
        QCOMPARE(out.pixelColor(350, 50), kFallback);
        QCOMPARE(out.pixelColor(200, 150), kFallback);

        // Nothing is drawn outside the item.
        item.setWidthScale(1.0);
        item.setRect(0.0f, 0.0f, 1.0f, 0.5f);
        out = render(item, 400, 300);
        QCOMPARE(out.pixelColor(200, 140), kRed);
        QCOMPARE(out.pixelColor(200, 200), QColor(Qt::black));
    }

    void oldRecordKeepsIntervalNewItemTakes120()
    {
        WebImageItem old;
        old.setFetchEnabled(false);
        QVERIFY(old.deserialize(QStringLiteral(
            "WEBIMAGE|0|0|1|1|-1|0|300|#ff202020|https://example.invalid/a.png")));
        QCOMPARE(old.refreshInterval(), 300);
        QCOMPARE(old.widthScale(), 1.0);
        QVERIFY(!old.bypassCache());
        QVERIFY(!old.guid().isNull());
        QCOMPARE(old.url(), QStringLiteral("https://example.invalid/a.png"));

        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("WEBIMAGE"));
        std::unique_ptr<MeterItem> created(
            registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
        auto* fresh = qobject_cast<WebImageItem*>(created.get());
        QVERIFY(fresh);
        QCOMPARE(fresh->refreshInterval(), 120);
        QCOMPARE(fresh->widthScale(), 1.0);
        QVERIFY(!fresh->guid().isNull());
    }

    void recordKeepsSettingsAndId()
    {
        WebImageItem item;
        item.setFetchEnabled(false);
        item.setGuid(kGuid);
        QCOMPARE(item.fourChar(), QStringLiteral("web4"));
        QCOMPARE(item.fourChar(), fourChar(QStringLiteral("webimage"), 0, kGuid));
        item.setUrl(QStringLiteral("https://example.invalid/a%2Fb.png?x=1|2"));
        item.setRefreshInterval(5);
        item.setWidthScale(0.25);
        item.setBypassCache(true);
        const QString record = item.serialize();
        QVERIFY(record.endsWith(kGuid.toString(QUuid::WithoutBraces)));

        WebImageItem copy;
        copy.setFetchEnabled(false);
        QVERIFY(copy.deserialize(record));
        QCOMPARE(copy.url(), item.url());
        QCOMPARE(copy.refreshInterval(), 5);
        QCOMPARE(copy.widthScale(), 0.25);
        QVERIFY(copy.bypassCache());
        QCOMPARE(copy.guid(), kGuid);
        QCOMPARE(copy.fourChar(), QStringLiteral("web4"));
        QCOMPARE(copy.serialize(), record);

        // Two new items get different IDs.
        WebImageItem a;
        WebImageItem b;
        QVERIFY(a.fourChar() != b.fourChar() || a.guid() != b.guid());
    }

    void previewNeverLoadsLiveCopyDoes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        WebImageItem source;
        source.setFetchEnabled(false);
        source.setUrl(writePng(dir, QStringLiteral("p.png"), 10, 10));
        QVERIFY(source.currentImage().isNull());
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.captureMeterItem(source);

        std::unique_ptr<MeterItem> preview(registry.createMeterItem(entry, nullptr, ContentRenderMode::Preview));
        auto* previewItem = qobject_cast<WebImageItem*>(preview.get());
        QVERIFY(previewItem);
        QVERIFY(!previewItem->fetchEnabled());
        QVERIFY(previewItem->currentImage().isNull());
        QCOMPARE(previewItem->fourChar(), source.fourChar());

        std::unique_ptr<MeterItem> live(registry.createMeterItem(entry, nullptr));
        auto* liveItem = qobject_cast<WebImageItem*>(live.get());
        QVERIFY(liveItem);
        QVERIFY(!liveItem->currentImage().isNull());
        QCOMPARE(liveItem->networkRequestCount(), 0);
        QCOMPARE(liveItem->fourChar(), source.fourChar());
    }

    void loopbackFetchStateAndBypass()
    {
        LoopbackServer server(200, pngBytes());
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QString url = QStringLiteral("http://127.0.0.1:%1/image.png").arg(server.serverPort());

        WebImageItem item;
        item.setFetchEnabled(false);
        item.setBypassCache(true);
        item.setUrl(url);
        item.setFetchEnabled(true);
        QCOMPARE(item.networkRequestCount(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(item.stateText(), QStringLiteral("ok"), 5000);
        QCOMPARE(item.currentImage().size(), QSize(8, 4));
        QCOMPARE(server.requestLines.size(), 1);
        QVERIFY2(server.requestLines[0].contains(QStringLiteral("/image.png?nereus_id=")),
                 qPrintable(server.requestLines[0]));

        LoopbackServer missing(404, QByteArray("gone"));
        QVERIFY(missing.listen(QHostAddress::LocalHost));
        WebImageItem bad;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^Failed to fetch image from")));
        bad.setUrl(QStringLiteral("http://127.0.0.1:%1/none.png").arg(missing.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(bad.stateText(), QStringLiteral("url issue"), 5000);
        QVERIFY(bad.currentImage().isNull());
        QCOMPARE(missing.requestLines.size(), 1);
        QVERIFY(!missing.requestLines[0].contains(QStringLiteral("nereus_id")));
    }
};

QTEST_MAIN(TestWebImageItem)
#include "tst_web_image_item.moc"
