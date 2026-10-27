// no-port-check: NereusSDR-original test for the workspace preview's
// decided status and data colours (src/gui/styles/WorkspacePreview
// statusColour and dataBackground) and the call sites that read them.
//
// Covers: with the switch off every site gives the legacy colour byte for
// byte, HTML text included. With it on, the status sites take Longpath's
// indicator tones (ok live, warn measured, danger tx) and the pulse's
// dimmer step is the bright tone mixed with titlebar-mid; the data
// backgrounds take the container role, and a background the user set on a
// container shows through the meters, dials and strips inside it. A colour
// the user set on an item or display still wins over both (the
// user-setting rule). Meter ink is unchanged.
//
// Modification history (NereusSDR):
//   2026-10-08 - Written for the workspace colour choices. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - JJ's decisions (status B, data B) are the product: the
//                 choice rows are gone; the pulse's two steps, the user
//                 container background and user item colours are covered.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QImage>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPixmap>

#include <cmath>

#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "gui/NetworkDiagnosticsDialog.h"
#include "gui/SMeterWidget.h"
#include "gui/TimeSeriesGraphWidget.h"
#include "gui/TitleBar.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/ItemColourLook.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "gui/widgets/StatusBadge.h"

using namespace NereusSDR;

namespace {

QColor role(const char* name)
{
    return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(name));
}

QImage render(QWidget& w)
{
    w.ensurePolished();
    return w.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

QColor firstOpaquePixel(const QPixmap& pm)
{
    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() == 255) {
                return c;
            }
        }
    }
    return {};
}

QPixmap badgeIcon(StatusBadge& badge)
{
    for (QLabel* l : badge.findChildren<QLabel*>()) {
        if (!l->pixmap().isNull()) {
            return l->pixmap();
        }
    }
    return {};
}

int countPixels(const QImage& img, const QRect& r, const QColor& c)
{
    int n = 0;
    const QRect clip = r.intersected(img.rect());
    for (int y = clip.top(); y <= clip.bottom(); ++y) {
        for (int x = clip.left(); x <= clip.right(); ++x) {
            if ((img.pixel(x, y) & 0x00ffffffu) == (c.rgb() & 0x00ffffffu)) {
                ++n;
            }
        }
    }
    return n;
}

QString statusText(NetworkDiagnosticsDialog& dlg)
{
    for (QLabel* l : dlg.findChildren<QLabel*>()) {
        if (l->text().contains(QStringLiteral("Disconnected"))
            && l->text().contains(QStringLiteral("<span"))) {
            return l->text();
        }
    }
    return {};
}

// The colour a status site shows: legacy with the switch off, the
// indicator tone with it on.
QColor expectedStatus(bool on, const QColor& legacy, const char* tone)
{
    return on ? role(tone) : legacy;
}

QColor expectedData(bool on, const QColor& legacy, const char* roleName)
{
    return on ? role(roleName) : legacy;
}

// The pulse's dimmer step: the bright tone mixed 25 % with titlebar-mid.
QColor dimStep(const char* tone)
{
    const QColor b = role(tone);
    const QColor m = role("titlebar-mid");
    const auto mix = [](int x, int y) { return static_cast<int>(std::lround(x * 0.75 + y * 0.25)); };
    return QColor(mix(b.red(), m.red()), mix(b.green(), m.green()), mix(b.blue(), m.blue()));
}

void addSwitchRows()
{
    QTest::addColumn<bool>("on");
    QTest::newRow("off") << false;
    QTest::newRow("on") << true;
}

const QColor kUserBg(0x2a, 0x1e, 0x14);

// A bar row's BarItem as ItemGroup::buildBarRow builds it, at -10 with
// the red threshold at 0, painted into an image.
QColor barBaseline(const QColor& barColour)
{
    BarItem bar;
    bar.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bar.setOrientation(BarItem::Orientation::Horizontal);
    bar.setRange(-30.0, 12.0);
    bar.setBarStyle(BarItem::BarStyle::Line);
    bar.setBarColor(barColour);
    bar.setBarRedColor(QColor(Qt::red));
    bar.setRedThreshold(0.0);
    bar.clearSmoothing(-10.0);
    QImage img(300, 40, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    bar.paint(p, img.width(), img.height());
    p.end();
    return img.pixelColor(5, 37);
}

QColor scaleTick(const QColor& tickColour)
{
    ScaleItem scale;
    scale.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    scale.setRange(-30.0, 12.0);
    scale.setMajorTicks(2);
    scale.setMinorTicks(0);
    if (tickColour.isValid()) {
        scale.setTickColor(tickColour);
    }
    QImage img(300, 40, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    scale.paint(p, img.width(), img.height());
    p.end();
    return img.pixelColor(0, 2);
}

// A Thetis bar strip (BarPresetItem, the SIGNALAVG variant) with no
// reading, painted on black: the backdrop, the scale (white to the red
// threshold, red past it), the "--" value text in the marker colour and the
// "--" peak text in red. `config` is applied first (user colours).
QImage presetStrip(const QJsonObject& config = {}, QObject* parent = nullptr)
{
    BarPresetItem strip(parent);
    strip.configureVariant(QStringLiteral("SignalAvg"));
    strip.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (!config.isEmpty()) {
        strip.applyConfiguration(config);
    }
    QImage img(400, 100, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    strip.paint(p, img.width(), img.height());
    p.end();
    return img;
}

} // namespace

class TestWorkspaceColourChoice : public QObject
{
    Q_OBJECT

private slots:
    void cleanup()
    {
        WorkspacePreview::setEnabledForTest(false);
    }

    // ── The helpers ─────────────────────────────────────────────────────

    void statusColour_data() { addSwitchRows(); }
    void statusColour()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        struct Case { QColor legacy; const char* role; const char* tone; };
        const Case cases[] = {
            {QColor(0x5f, 0xff, 0x8a), "ok", "live"},
            {QColor(0xff, 0xd7, 0x00), "warn", "measured"},
            {QColor(0xff, 0x60, 0x60), "danger", "tx"},
            {QColor(0x5f, 0xa8, 0xff), "accent", "accent"},
        };
        for (const Case& c : cases) {
            const QColor out = WorkspacePreview::statusColour(c.legacy, c.role);
            QCOMPARE(out.rgba(), expectedStatus(on, c.legacy, c.tone).rgba());
        }
        if (on) {
            // The decided tones, by value.
            QCOMPARE(WorkspacePreview::statusColour(QColor(Qt::black), "ok").name(), QStringLiteral("#4caf6a"));
            QCOMPARE(WorkspacePreview::statusColour(QColor(Qt::black), "warn").name(), QStringLiteral("#d8a55f"));
            QCOMPARE(WorkspacePreview::statusColour(QColor(Qt::black), "danger").name(), QStringLiteral("#c25a5c"));
        }
        // A translucent legacy colour keeps its alpha.
        const QColor translucent(0x5f, 0xff, 0x8a, 26);
        QCOMPARE(WorkspacePreview::statusColour(translucent, "ok").alpha(), 26);
    }

    void statusColourName_data() { addSwitchRows(); }
    void statusColourName()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        const QString out = WorkspacePreview::statusColourName("#3DD068", "ok");
        QCOMPARE(out, on ? role("live").name() : QStringLiteral("#3DD068"));
    }

    void dataBackground_data() { addSwitchRows(); }
    void dataBackground()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        const QColor legacy(0x0f, 0x0f, 0x1a);
        QCOMPARE(WorkspacePreview::dataBackground(legacy, "panel").rgba(),
                 expectedData(on, legacy, "panel").rgba());
        const QColor veil(0x0f, 0x0f, 0x1a, 170);
        QCOMPARE(WorkspacePreview::dataBackground(veil, "panel").alpha(), 170);
    }

    // The user's container background, read from the surface's ancestors:
    // a meter host inside a container, and a meter item whose parent is
    // that host (in the app, MeterWidget). With the default background the role applies; with
    // the switch off the legacy colour does.
    void dataBackgroundReadsUserContainer_data() { addSwitchRows(); }
    void dataBackgroundReadsUserContainer()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        const QColor legacy(0x0f, 0x0f, 0x1a);
        ContainerWidget container;
        auto* meter = new QWidget; // MeterWidget's place (a QRhiWidget: no offscreen paint)
        container.setContent(meter);
        BarPresetItem item(meter);
        QCOMPARE(WorkspacePreview::dataBackground(legacy, "panel", meter).rgba(),
                 expectedData(on, legacy, "panel").rgba());
        container.setBackgroundColor(kUserBg);
        const QColor userOrLegacy = on ? kUserBg : legacy;
        QCOMPARE(WorkspacePreview::dataBackground(legacy, "panel", meter).rgba(), userOrLegacy.rgba());
        QCOMPARE(WorkspacePreview::dataBackground(legacy, "panel", &item).rgba(), userOrLegacy.rgba());
        // The unavailable veil keeps its alpha over the user's colour.
        const QColor veil = WorkspacePreview::dataBackground(QColor(0x0f, 0x0f, 0x1a, 170), "panel", meter);
        QCOMPARE(veil.alpha(), 170);
        if (on) {
            QCOMPARE(veil.rgb(), kUserBg.rgb());
        }
        // Back to the default background: the role again.
        container.setBackgroundColor(QColor::fromRgba(0xff0f0f1a));
        QCOMPARE(WorkspacePreview::dataBackground(legacy, "panel", meter).rgba(),
                 expectedData(on, legacy, "panel").rgba());
    }

    // ── Status call sites ───────────────────────────────────────────────

    void connectionDot_data() { addSwitchRows(); }
    void connectionDot()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        struct Case { ConnectionState state; QColor legacy; const char* tone; };
        const Case cases[] = {
            {ConnectionState::Connected, QColor(0x3f, 0xcf, 0x6a), "live"},
            {ConnectionState::LinkLost, QColor(0xcf, 0x6c, 0x00), "measured"},
            {ConnectionState::Disconnected, QColor(0xff, 0x40, 0x40), "tx"},
        };
        for (const Case& c : cases) {
            ConnectionSegment seg;
            seg.resize(360, 24);
            seg.setState(c.state);
            seg.setPulsePausedForTest(true);
            const QImage img = render(seg);
            const QColor dot = img.pixelColor(13, 12);
            const bool pulses = c.state != ConnectionState::Disconnected;
            const QColor expected = !on ? c.legacy : (pulses ? dimStep(c.tone) : role(c.tone));
            QCOMPARE(dot.rgb(), expected.rgb());
        }
    }

    // The pulse's two steps differ: with the switch on the brighter step is
    // the indicator tone and the dimmer step the tone mixed with
    // titlebar-mid; off, the legacy pair.
    void pulseStepsDiffer_data() { addSwitchRows(); }
    void pulseStepsDiffer()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        struct Case { ConnectionState state; QColor legacyBright; QColor legacyDim; const char* tone; };
        const Case cases[] = {
            {ConnectionState::Connected, QColor(0x5f, 0xff, 0x8a), QColor(0x3f, 0xcf, 0x6a), "live"},
            {ConnectionState::LinkLost, QColor(0xff, 0x8c, 0x00), QColor(0xcf, 0x6c, 0x00), "measured"},
        };
        for (const Case& c : cases) {
            ConnectionSegment seg;
            seg.resize(360, 24);
            seg.setState(c.state);
            seg.setPulsePausedForTest(true, true);
            const QColor bright = render(seg).pixelColor(13, 12);
            seg.setPulsePausedForTest(true, false);
            const QColor dim = render(seg).pixelColor(13, 12);
            QVERIFY2(bright.rgb() != dim.rgb(), qPrintable(bright.name()));
            QCOMPARE(bright.rgb(), (on ? role(c.tone) : c.legacyBright).rgb());
            QCOMPARE(dim.rgb(), (on ? dimStep(c.tone) : c.legacyDim).rgb());
            if (on) {
                // Darker, same hue (within a couple of degrees).
                QVERIFY(dim.value() < bright.value());
                QVERIFY(std::abs(dim.hsvHue() - bright.hsvHue()) <= 2);
            }
        }
        if (on) {
            QCOMPARE(dimStep("live").name(), QStringLiteral("#408a58"));
        }
    }

    void latencyNumber_data() { addSwitchRows(); }
    void latencyNumber()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        struct Case { int rtt; QColor legacy; const char* tone; };
        const Case cases[] = {
            {12, QColor(0x5f, 0xff, 0x8a), "live"},
            {80, QColor(0xff, 0xd7, 0x00), "measured"},
            {250, QColor(0xff, 0x60, 0x60), "tx"},
        };
        for (const Case& c : cases) {
            ConnectionSegment seg;
            seg.resize(360, 24);
            seg.setState(ConnectionState::Connected);
            seg.setPulsePausedForTest(true);
            seg.setRates(18.4, 0.6);
            seg.setRttMs(c.rtt);
            const QImage img = render(seg);
            const QRect rtt = seg.rttRect();
            QVERIFY(rtt.width() > 0);
            const QColor expected = expectedStatus(on, c.legacy, c.tone);
            QVERIFY2(countPixels(img, rtt, expected) > 0,
                     qPrintable(QStringLiteral("rtt %1: no pixel in %2").arg(c.rtt)
                                    .arg(expected.name())));
            if (expected.rgb() != c.legacy.rgb()) {
                QCOMPARE(countPixels(img, rtt, c.legacy), 0);
            }
        }
    }

    void networkStatus_data() { addSwitchRows(); }
    void networkStatus()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        NetworkDiagnosticsDialog dlg(nullptr, nullptr);
        QTest::qWait(300);
        const QString text = statusText(dlg);
        QVERIFY2(!text.isEmpty(), "no Disconnected status label");
        const QString expected = on ? role("tx").name() : QStringLiteral("#ff6060");
        QVERIFY2(text.contains(QStringLiteral("color:%1;").arg(expected)), qPrintable(text));
    }

    void statusBadge_data() { addSwitchRows(); }
    void statusBadge()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        StatusBadge badge;
        badge.setVariant(StatusBadge::Variant::On);
        badge.setSvgIcon(QStringLiteral(":/icons/badge-mode.svg"));
        const QColor legacy(0x5f, 0xff, 0x8a);
        const QColor expected = expectedStatus(on, legacy, "live");
        QCOMPARE(firstOpaquePixel(badgeIcon(badge)).rgb(), expected.rgb());

        // With the switch on the badge writes its resolved colours and keeps
        // the stylesheet pass out; off, it writes today's sheet.
        QCOMPARE(badge.property(WorkspacePreview::kSkipProperty).toBool(), on);
        const QString sheet = badge.styleSheet();
        if (on) {
            QVERIFY2(sheet.contains(QStringLiteral("color: %1;").arg(expected.name())),
                     qPrintable(sheet));
        } else {
            QVERIFY2(sheet.contains(QStringLiteral("color: #5fff8a;")), qPrintable(sheet));
            QVERIFY2(sheet.contains(QStringLiteral("rgba(95,255,138,26)")), qPrintable(sheet));
        }
    }

    // ── Data call sites ─────────────────────────────────────────────────

    void filterPassband_data() { addSwitchRows(); }
    void filterPassband()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        FilterPassbandWidget w;
        w.resize(200, 80);
        const QImage img = render(w);
        // Inside the 1 px border.
        QCOMPARE(img.pixelColor(3, 3).rgb(), expectedData(on, QColor(0x0a, 0x0a, 0x18), "panel").rgb());
    }

    void timeSeriesGraph_data() { addSwitchRows(); }
    void timeSeriesGraph()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        TimeSeriesGraphWidget w(QStringLiteral("RTT"), QStringLiteral("ms"));
        w.resize(300, 160);
        const QImage img = render(w);
        QCOMPARE(img.pixelColor(1, img.height() - 2).rgb(),
                 expectedData(on, QColor(0x05, 0x0b, 0x13), "app-bg").rgb());
    }

    void classicDialBackground_data() { addSwitchRows(); }
    void classicDialBackground()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        SMeterWidget w(SMeterWidget::SettingsStore::Instance);
        w.setFaceStyle(SMeterWidget::FaceStyle::Classic);
        w.resize(300, 150);
        const QImage img = render(w);
        QCOMPARE(img.pixelColor(0, 0).rgb(), expectedData(on, QColor(0x0f, 0x0f, 0x1a), "panel").rgb());
    }

    void parametricEq_data() { addSwitchRows(); }
    void parametricEq()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        ParametricEqWidget w;
        w.setEditorPresentationEnabled(true);
        w.resize(500, 300);
        const QImage img = render(w);
        QCOMPARE(img.pixelColor(1, 1).rgb(), expectedData(on, QColor(0x0f, 0x0f, 0x1a), "app-bg").rgb());
    }

    // Meter ink: a bar row's white bar (no listed default) is unchanged in
    // both states; the scale's factory tick colour follows the workspace
    // look with the switch on (ItemColourLook, 2026-10-26) and is classic
    // with it off.
    void meterInkUnchanged_data() { addSwitchRows(); }
    void meterInkUnchanged()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        QCOMPARE(barBaseline(QColor(Qt::white)).rgb(), QColor(Qt::white).rgb());
        QCOMPARE(scaleTick(QColor()).rgb(),
                 ItemColourLook::effective("Scale.tickColor", QColor(0xc8, 0xd8, 0xe8)).rgb());
        if (on) {
            QVERIFY(scaleTick(QColor()).rgb() != QColor(0xc8, 0xd8, 0xe8).rgb());
        }
    }

    void userColoursWin_data() { addSwitchRows(); }
    void userColoursWin()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        const QColor userBar(0x00, 0xff, 0x00);
        QCOMPARE(barBaseline(userBar).rgb(), userBar.rgb());
        const QColor userTick(0x20, 0x80, 0xff);
        QCOMPARE(scaleTick(userTick).rgb(), userTick.rgb());
    }

    // The Thetis bar strips (BarPresetItem): the default (32,32,32)
    // backdrop takes the panel role with the switch on; the factory ink
    // (white scale, red over-range, yellow marker) follows the workspace
    // look with the switch on (ItemColourLook, 2026-10-26) and is classic
    // with it off.
    void barStrip_data() { addSwitchRows(); }
    void barStrip()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        const QImage img = presetStrip();
        QCOMPARE(img.pixelColor(1, 1).rgb(), expectedData(on, QColor(32, 32, 32), "panel").rgb());
        for (const QColor& ink : {ItemColourLook::effective("BarPreset.lowColor", QColor(Qt::white)),
                                  ItemColourLook::effective("BarPreset.highColor", QColor(Qt::red)),
                                  ItemColourLook::effective("BarPreset.barColor", QColor(Qt::yellow))}) {
            QVERIFY2(countPixels(img, img.rect(), ink) > 0, qPrintable(ink.name()));
        }
    }

    // A user container background shows through the meters inside it: a
    // bar strip whose host sits in the container, and a Classic dial
    // (the container's content) painted in place.
    void userContainerShowsThroughMeters_data() { addSwitchRows(); }
    void userContainerShowsThroughMeters()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        ContainerWidget container;
        container.resize(320, 200);
        auto* meter = new QWidget; // MeterWidget's place (a QRhiWidget: no offscreen paint)
        container.setContent(meter);
        container.setBackgroundColor(kUserBg);
        const QImage strip = presetStrip({}, meter);
        QCOMPARE(strip.pixelColor(1, 1).rgb(), (on ? kUserBg : QColor(32, 32, 32)).rgb());

        ContainerWidget dialContainer;
        dialContainer.resize(320, 200);
        auto* dial = new SMeterWidget(SMeterWidget::SettingsStore::Instance);
        dial->setFaceStyle(SMeterWidget::FaceStyle::Classic);
        dialContainer.setContent(dial);
        dialContainer.setBackgroundColor(kUserBg);
        dial->resize(300, 150);
        const QImage img = render(*dial);
        QCOMPARE(img.pixelColor(1, 1).rgb(), (on ? kUserBg : QColor(0x0f, 0x0f, 0x1a)).rgb());
    }

    // A colour the user set on the item still wins over the user's
    // container background and the role.
    void barStripUserColours_data() { addSwitchRows(); }
    void barStripUserColours()
    {
        QFETCH(bool, on);
        WorkspacePreview::setEnabledForTest(on);
        ContainerWidget container;
        auto* meter = new QWidget; // MeterWidget's place (a QRhiWidget: no offscreen paint)
        container.setContent(meter);
        container.setBackgroundColor(kUserBg);
        const QColor userBackdrop(0x14, 0x28, 0x3c);
        const QColor userBar(0x00, 0xff, 0x00);
        const QImage img = presetStrip({{QStringLiteral("backdropColor"), userBackdrop.name()},
                                        {QStringLiteral("barColor"), userBar.name()}},
                                       meter);
        QCOMPARE(img.pixelColor(1, 1).rgb(), userBackdrop.rgb());
        QVERIFY(countPixels(img, img.rect(), userBar) > 0);
    }
};

QTEST_MAIN(TestWorkspaceColourChoice)
#include "tst_workspace_colour_choice.moc"
