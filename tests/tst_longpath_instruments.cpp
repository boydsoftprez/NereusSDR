// no-port-check: NereusSDR-original checks for the ported Longpath
// instruments and their container-content registration (workspace preview).
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - frequencyRowUsesSliceLetters. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Needles read their own context through the poller's
//                 reading consumers. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - frequencyFollowsASliceAddedLater,
//                 barFooterMakesRoomForTheSecondTrough and
//                 noMeterReadingClearsTheSecondary. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - previewNeedleIsInertAndSeeded and
//                 registrationReplayReachesOnlyTheNewWidget. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - editorShowsDeclaredProperties,
//                 compactAndLabelReachTheWidgets and
//                 sourceMenuCommitsTheContainerSource. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Review C: the source menu tick for a sliceId config,
//                 Advanced disabled for an unavailable declared type, named
//                 reading ids. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - The source chip case no longer calls the removed header
//                 preview seam; its header is shown. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - barFormsShowTheSameTextForNoReading. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-20 - Frequency (Custom) font, colour, centering and slice lines.
//                 Brian Sutton, AI-assisted via xAI Grok.
//   2026-10-23 - frequencyShowsAfterTheStationAdoptsTheSlice. Brian Sutton,
//                 AI-assisted via xAI Grok.
//   2026-10-24 - frequencyShowsWhenRemoteSliceAccessArrives. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <cstring>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QMenu>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTimer>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QSplitter>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "core/antenna/AmateurBands.h"
#include "core/session/SliceAccessPolicy.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/LongpathContent.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// The rich text of the instrument's VFO row (the only label with two
// <font> runs).
QString vfoRowText(const QWidget& w)
{
    for (const QLabel* l : w.findChildren<QLabel*>()) {
        if (l->text().count(QStringLiteral("<font")) == 2) { return l->text(); }
    }
    return {};
}

// A paint device that keeps the text drawn on it and nothing else.
class TextObserverEngine final : public QPaintEngine {
public:
    TextObserverEngine() : QPaintEngine(AllFeatures) {}
    QStringList texts;
    bool begin(QPaintDevice* device) override
    {
        setPaintDevice(device);
        setActive(true);
        return true;
    }
    bool end() override
    {
        setActive(false);
        return true;
    }
    void updateState(const QPaintEngineState&) override {}
    Type type() const override { return User; }
    void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) override {}
    void drawImage(const QRectF&, const QImage&, const QRectF&, Qt::ImageConversionFlags) override {}
    void drawPath(const QPainterPath&) override {}
    void drawPolygon(const QPointF*, int, PolygonDrawMode) override {}
    void drawTextItem(const QPointF&, const QTextItem& item) override { texts.append(item.text()); }
};
class TextObserverDevice final : public QPaintDevice {
public:
    explicit TextObserverDevice(QSize size) : m_size(size) {}
    QPaintEngine* paintEngine() const override { return &m_engine; }
    const QStringList& texts() const { return m_engine.texts; }

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case PdmWidth: return m_size.width();
        case PdmHeight: return m_size.height();
        case PdmDpiX: case PdmDpiY: case PdmPhysicalDpiX: case PdmPhysicalDpiY: return 96;
        case PdmDepth: return 32;
        case PdmDevicePixelRatio: return 1;
        case PdmDevicePixelRatioScaled: return int(devicePixelRatioFScale());
        default: return QPaintDevice::metric(metric);
        }
    }

private:
    QSize m_size;
    mutable TextObserverEngine m_engine;
};

// The text a bar draws at `size` (its footer is a child widget and draws
// its own).
QStringList textsDrawnBy(BarInstrument& bar, QSize size)
{
    TextObserverDevice device(size);
    QPainter painter(&device);
    bar.paintInto(painter, size, false);
    painter.end();
    return device.texts();
}

// The value at the right of an instrument's footer: the last label in it.
QString footerValueText(const QWidget& instrument)
{
    const auto* footer = instrument.findChild<InstrumentFooter*>();
    if (!footer || !footer->layout() || footer->layout()->count() == 0) { return {}; }
    const auto* value = qobject_cast<QLabel*>(footer->layout()->itemAt(footer->layout()->count() - 1)->widget());
    return value ? value->text() : QString();
}

} // namespace

// Longpath's NoiseFloor and RadeSnr reading ids, kept local in
// ReadingSource.cpp (LongpathBinding); MeterBinding has neither.
constexpr int kNoiseFloorReading = 9;
constexpr int kRadeSnrReading = 10;

class TstLongpathInstruments : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WorkspacePreview::setEnabledForTest(true); }
    void cleanupTestCase() { WorkspacePreview::setEnabledForTest(false); }

    void frequencyInstrumentSizesAtDefaultForm()
    {
        FrequencyInstrument w;
        QCOMPARE(w.form(), FrequencyInstrument::Form::NumberOnly);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        settle();
        QVERIFY(w.vfoRowVisible());
        // Longpath FrequencyInstrument.cpp:194-216: {max(240, digit row at
        // 12 px + 28), 74} with the A/B row shown. At 12 px the digit row
        // is well under 212 px, so the documented floor holds.
        QCOMPARE(w.minimumSizeHint(), QSize(240, 74));
        // FrequencyInstrument.cpp:218-253: {max(360, layout), max(40, layout)}.
        const QSize fromLayout = w.layout()->sizeHint();
        QCOMPARE(w.sizeHint(), QSize(qMax(360, fromLayout.width()), qMax(40, fromLayout.height())));
        QCOMPARE(w.sizeHint().width(), 360);
        // Hiding the A/B row gives its height back to the minimum.
        w.setVfoRowVisible(false);
        QVERIFY(w.minimumSizeHint().height() < 74);
        QVERIFY(w.minimumSizeHint().height() >= 40);
    }

    void stripFormsReserveTheirStripAtMinimum()
    {
        // NereusSDR fix: the BandStrip / FlatArc reservation is part of the
        // minimum height, so the digit row is not squeezed at the minimum.
        FrequencyInstrument number;
        FrequencyInstrument strip;
        strip.setForm(FrequencyInstrument::Form::BandStrip);
        FrequencyInstrument arc;
        arc.setForm(FrequencyInstrument::Form::FlatArc);
        for (FrequencyInstrument* w : {&number, &strip, &arc}) {
            w->show();
            QVERIFY(QTest::qWaitForWindowExposed(w));
        }
        settle();
        QVERIFY(strip.minimumSizeHint().height() > number.minimumSizeHint().height());
        QVERIFY(arc.minimumSizeHint().height() > strip.minimumSizeHint().height());
        QVERIFY(strip.minimumSizeHint().height() <= strip.sizeHint().height());
        QVERIFY(arc.minimumSizeHint().height() <= arc.sizeHint().height());
    }

    void barFooterOnlyWhenItFits()
    {
        // NereusSDR fix: the footer shows at the preferred size and is
        // hidden at the minimum instead of covering the trough.
        BarInstrument bar;
        bar.setPrimary(MeterBinding::TxSwr);
        bar.onReading(MeterBinding::TxSwr, 1.25);
        bar.resize(bar.sizeHint());
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        settle();
        auto* footer = bar.findChild<InstrumentFooter*>();
        QVERIFY(footer);
        QVERIFY(footer->isVisible());
        QVERIFY(bar.sizeHint().height() >= 74);
        bar.resize(bar.minimumSizeHint());
        settle();
        QVERIFY(!footer->isVisible());
        bar.resize(bar.sizeHint());
        settle();
        QVERIFY(footer->isVisible());
    }

    void barFormsShowTheSameTextForNoReading()
    {
        // Forward power large beside reflected power and SWR slim: with
        // nothing measured the full form's footer and the compact form's
        // value line show the same single dash, the one the needle's
        // footer shows too.
        const QString dash(QChar(0x2014));
        BarInstrument full;
        QVERIFY(full.setPrimary(MeterBinding::TxSwr));
        BarInstrument compact;
        compact.setCompact(true);
        QVERIFY(compact.setPrimary(MeterBinding::TxSwr));
        QVERIFY(compact.setSecondary(MeterBinding::TxPower));
        NeedleInstrument needle;
        QVERIFY(needle.setPrimary(MeterBinding::SignalPeak));
        const QSize slim(320, compact.sizeHint().height());

        const auto compactPlaceholders = [&] {
            QStringList drawn = textsDrawnBy(compact, slim);
            // What is left beside the two names is the two values.
            const int before = drawn.size();
            drawn.removeAll(readingFor(MeterBinding::TxSwr)->thetisName());
            drawn.removeAll(readingFor(MeterBinding::TxPower)->thetisName());
            return before - drawn.size() == 2 ? drawn : QStringList{QStringLiteral("names not drawn")};
        };
        QVERIFY(!full.hasValue() && !compact.hasValue() && !compact.hasSecondValue());
        QCOMPARE(footerValueText(full), dash);
        QCOMPARE(footerValueText(needle), dash);
        QCOMPARE(compactPlaceholders(), (QStringList{dash, dash}));

        // With readings each shows its value; cleared, the dash again.
        full.onReading(MeterBinding::TxSwr, 1.25);
        compact.onReading(MeterBinding::TxSwr, 1.25);
        compact.onReading(MeterBinding::TxPower, 50.0);
        const QString swr = readingFor(MeterBinding::TxSwr)->textWithUnit(1.25);
        QVERIFY(swr != dash);
        QCOMPARE(footerValueText(full), swr);
        QCOMPARE(compactPlaceholders(), (QStringList{swr, readingFor(MeterBinding::TxPower)->textWithUnit(50.0)}));
        full.clearValue();
        compact.clearValue();
        QCOMPARE(footerValueText(full), dash);
        QCOMPARE(compactPlaceholders(), (QStringList{footerValueText(full), footerValueText(full)}));
    }

    void barFooterMakesRoomForTheSecondTrough()
    {
        // NereusSDR fix: with a secondary reading the second trough takes
        // part of the height, so the footer needs more to fit. Setting the
        // secondary at a fixed size hides the footer at once.
        BarInstrument bar;
        bar.setPrimary(MeterBinding::TxSwr);
        bar.onReading(MeterBinding::TxSwr, 1.25);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        auto* footer = bar.findChild<InstrumentFooter*>();
        QVERIFY(footer);
        bar.resize(bar.sizeHint());
        settle();
        QVERIFY(footer->isVisible());
        QVERIFY(bar.setSecondary(MeterBinding::TxPower));
        settle();
        QVERIFY(!footer->isVisible());
        bar.resize(bar.minimumSizeHint());
        settle();
        QVERIFY(!footer->isVisible());
        bar.resize(bar.sizeHint());
        settle();
        QVERIFY(footer->isVisible());
        QVERIFY(bar.setSecondary(-1));
        settle();
        QVERIFY(footer->isVisible());
    }

    void frequencyGroupingAndParsing()
    {
        FrequencyInstrument w;
        w.setFrequency(14225000.0);
        QCOMPARE(w.groupedText(), QStringLiteral("14.225.000"));
        w.setFrequency(7185000.0);
        QCOMPARE(w.groupedText(), QStringLiteral("7.185.000"));
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7.230.000")), 7230000.0);
        QCOMPARE(FrequencyInstrument::parseUserFrequency(QStringLiteral("7230")), 7230000.0);
        QCOMPARE(AmateurBands::containing(14225000.0).name, QStringLiteral("20 m"));
    }

    void bandwidthFilterPaneSizes()
    {
        BandwidthFilterPane pane;
        QCOMPARE(pane.minimumSize(), QSize(180, 84));
        QCOMPARE(pane.sizeHint(), QSize(360, 140));
    }

    void needleWithoutReadingIsOffline()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* needle = qobject_cast<NeedleInstrument*>(widget.get());
        QVERIFY(needle);
        QCOMPARE(needle->primary(), int(MeterBinding::SignalPeak));
        QVERIFY(!needle->hasValue());
        QVERIFY(needle->isOffline());
    }

    void readingsWithoutProducerAreUnavailable()
    {
        // NoiseFloor (9) and RadeSnr (10) keep their descriptors but have no
        // NereusSDR producer.
        for (int id : {kNoiseFloorReading, kRadeSnrReading}) {
            const ReadingDescriptor* d = readingFor(id);
            QVERIFY(d);
            QVERIFY(d->hasScale);
            QVERIFY(!d->available);
        }
        QVERIFY(readingFor(MeterBinding::SignalPeak)->available);
        QVERIFY(readingFor(MeterBinding::TxSwr)->available);

        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        entry.config.insert(QString::fromLatin1(LongpathContent::kBindingKey), 9);
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* needle = qobject_cast<NeedleInstrument*>(widget.get());
        QVERIFY(needle);
        QVERIFY(needle->isOffline());
    }

    void txAlcHasThetisScale()
    {
        // NereusSDR addition: Thetis's ALC bar calibration (MeterManager.cs
        // AddALCBar): -30 dB at 0, 0 dB at 0.665, +12 dB at 0.99, red from 0.
        const ReadingDescriptor* d = readingFor(MeterBinding::TxAlc);
        QVERIFY(d);
        QVERIFY(d->hasScale);
        QVERIFY(d->available);
        QCOMPARE(d->unit, QStringLiteral("dB"));
        QCOMPARE(d->min, -30.0);
        QCOMPARE(d->max, 12.0);
        QVERIFY(d->threshold.has_value());
        QCOMPARE(*d->threshold, 0.0);
        QVERIFY(qAbs(d->fraction(-30.0)) < 1e-9);
        QVERIFY(qAbs(d->fraction(0.0) - 0.665) < 1e-9);
        QVERIFY(qAbs(d->fraction(12.0) - 0.99) < 1e-9);
        QVERIFY(qAbs(d->fraction(-15.0) - 0.3325) < 1e-9);
        QCOMPARE(d->ticks.size(), 6);
        QCOMPARE(d->minorTicks.size(), 6);

        BarInstrument bar;
        QVERIFY(bar.setPrimary(MeterBinding::TxAlc));
        bar.onReading(MeterBinding::TxAlc, -3.0);
        QVERIFY(bar.hasValue());
    }

    void registryRoundTripCreatesTwoNeedles()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        QStringList names;
        for (const auto& d : registry.descriptors()) {
            if (d.typeId.startsWith(QLatin1String("longpath."))) {
                QVERIFY(!d.singleton);
                QVERIFY(d.available);
                names.append(d.title);
            }
        }
        QCOMPARE(names, (QStringList{QStringLiteral("Frequency (Longpath)"), QStringLiteral("Frequency (Custom)"),
                                     QStringLiteral("Needle meter (Longpath)"), QStringLiteral("Bar meter (Longpath)"),
                                     QStringLiteral("Filter passband (Longpath)")}));

        ContentEntry rx = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        ContentEntry tx = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        QCOMPARE(rx.name, QStringLiteral("Needle meter (Longpath)"));
        QVERIFY(rx.id != tx.id);
        QVERIFY(registry.validateEntry(rx).isEmpty());
        tx.config.insert(QString::fromLatin1(LongpathContent::kDirectionKey), QStringLiteral("tx"));

        QWidget parent;
        std::unique_ptr<QWidget> a(registry.createWidget(rx, &parent));
        std::unique_ptr<QWidget> b(registry.createWidget(tx, &parent));
        auto* first = qobject_cast<NeedleInstrument*>(a.get());
        auto* second = qobject_cast<NeedleInstrument*>(b.get());
        QVERIFY(first && second && first != second);
        QCOMPARE(first->primary(), int(MeterBinding::SignalPeak));
        QCOMPARE(second->primary(), int(MeterBinding::TxPower));
        QCOMPARE(a->property("containerEntryId").toString(), rx.id);
        QCOMPARE(b->property("containerEntryId").toString(), tx.id);

        // Through a container host: one widget per entry, never shared.
        QTemporaryDir directory;
        AppSettings settings{directory.filePath("longpath.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContainerDocument c;
        c.id = QStringLiteral("station");
        c.name = QStringLiteral("Station");
        c.layout = ContentLayout::VerticalStack;
        c.dockMode = DockMode::OverlayDocked;
        c.geometry = QRect(10, 10, 380, 700);
        rx.id = QStringLiteral("station-rx");
        tx.id = QStringLiteral("station-tx");
        c.contents = {rx, tx};
        WorkspaceDocument document;
        document.containers.append(c);
        document.mainContainerId = c.id;
        const auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(c.id);
        QVERIFY(host);
        const auto needles = host->findChildren<NeedleInstrument*>();
        QCOMPARE(needles.size(), 2);
        QSet<int> primaries;
        for (auto* n : needles) { primaries.insert(n->primary()); }
        QCOMPARE(primaries, (QSet<int>{MeterBinding::SignalPeak, MeterBinding::TxPower}));
    }

    void frequencyRowUsesSliceLetters()
    {
        // Upstream defaults: A for this instrument, B for the other.
        FrequencyInstrument plain;
        QCOMPARE(plain.ownLetter(), QLatin1Char('A'));
        QCOMPARE(plain.otherLetter(), QLatin1Char('B'));

        // No radio here: a slice's frequency change warns that the TX
        // frequency is not pushed. Expected, so kept out of the output.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.warning=false"));
        const auto restoreRules = qScopeGuard([] { QLoggingCategory::setFilterRules(QString()); });
        RadioModel model;
        model.addSlice();
        model.addSlice();
        QCOMPARE(model.slices().size(), 2);
        SliceModel* sliceA = model.slices().at(0);
        SliceModel* sliceB = model.slices().at(1);
        QCOMPARE(sliceA->sliceLetter(), QLatin1Char('A'));
        QCOMPARE(sliceB->sliceLetter(), QLatin1Char('B'));
        sliceA->setFrequency(7185000.0);
        sliceB->setFrequency(14225000.0);

        SliceModel* target = sliceB;
        LongpathContent::Sources sources;
        sources.radio = &model;
        sources.sliceForContext = [&target](const QJsonObject&) { return target; };
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, sources);
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyType));

        QWidget parent;
        std::unique_ptr<QWidget> onB(registry.createWidget(entry, &parent));
        auto* freqB = qobject_cast<FrequencyInstrument*>(onB.get());
        QVERIFY(freqB);
        QCOMPARE(freqB->ownLetter(), QLatin1Char('B'));
        QCOMPARE(freqB->otherLetter(), QLatin1Char('A'));
        const QString rowB = vfoRowText(*freqB);
        QVERIFY2(rowB.contains(QStringLiteral(">B 14.225.000<")), qPrintable(rowB));
        QVERIFY2(rowB.contains(QStringLiteral(">A 7.185.000<")), qPrintable(rowB));
        QVERIFY(rowB.indexOf(QStringLiteral(">B ")) < rowB.indexOf(QStringLiteral(">A ")));

        target = sliceA;
        std::unique_ptr<QWidget> onA(registry.createWidget(entry, &parent));
        auto* freqA = qobject_cast<FrequencyInstrument*>(onA.get());
        QVERIFY(freqA);
        const QString rowA = vfoRowText(*freqA);
        QVERIFY2(rowA.contains(QStringLiteral(">A 7.185.000<")), qPrintable(rowA));
        QVERIFY2(rowA.contains(QStringLiteral(">B 14.225.000<")), qPrintable(rowA));
        QVERIFY(rowA.indexOf(QStringLiteral(">A ")) < rowA.indexOf(QStringLiteral(">B ")));
    }

    void frequencyFollowsASliceAddedLater()
    {
        // Built on slice A before slice B exists: adding B wires the other
        // row, and switching the active slice moves the active marker,
        // although the instrument's own slice never changes.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.warning=false"));
        const auto restoreRules = qScopeGuard([] { QLoggingCategory::setFilterRules(QString()); });
        RadioModel model;
        model.addSlice();
        QCOMPARE(model.slices().size(), 1);
        SliceModel* sliceA = model.slices().at(0);
        sliceA->setFrequency(7185000.0);

        LongpathContent::Sources sources;
        sources.radio = &model;
        sources.sliceForContext = [sliceA](const QJsonObject&) { return sliceA; };
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, sources);
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyType));
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* freq = qobject_cast<FrequencyInstrument*>(widget.get());
        QVERIFY(freq);
        QCOMPARE(freq->otherFrequency(), 0.0);
        QVERIFY(freq->activeIsThis());

        model.addSlice();
        QCOMPARE(model.slices().size(), 2);
        SliceModel* sliceB = model.slices().at(1);
        QCOMPARE(sliceB->sliceLetter(), QLatin1Char('B'));
        sliceB->setFrequency(14225000.0);
        QCOMPARE(freq->otherFrequency(), 14225000.0);
        QCOMPARE(freq->ownLetter(), QLatin1Char('A'));
        QCOMPARE(freq->otherLetter(), QLatin1Char('B'));
        QCOMPARE(freq->frequency(), 7185000.0);

        model.setActiveSlice(1);
        QVERIFY(sliceB->isActiveSlice());
        QVERIFY(!freq->activeIsThis());
        model.setActiveSlice(0);
        QVERIFY(sliceA->isActiveSlice());
        QVERIFY(freq->activeIsThis());

        // The own slice's binding survives the refreshes.
        sliceA->setFrequency(7190000.0);
        QCOMPARE(freq->frequency(), 7190000.0);
    }

    void frequencyShowsAfterTheStationAdoptsTheSlice()
    {
        // Direct connect, desktop hosting. The instrument is built before
        // the radio connects, so its sliceAdded slot is connected first.
        // The slice is published while nobody owns it. The station adopts
        // it from a later sliceAdded slot (StationSliceOwnershipPolicy).
        // That adoption does not emit activeSliceChanged: the slice is
        // already the active one. windowRxSlice only returns a slice the
        // station may change, so the first look finds nothing and the
        // digits stay on the empty reading unless ownership is followed.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.connection.warning=false"));
        const auto restoreRules = qScopeGuard([] { QLoggingCategory::setFilterRules(QString()); });
        RadioModel model;
        const QByteArray station = SliceOwnership::stationDevice();
        auto stationSlice = [&model, station](const QJsonObject&) -> SliceModel* {
            SliceOwnership* ownership = model.sliceOwnership();
            if (!ownership) { return nullptr; }
            const auto controlled = [&](int id) {
                return id >= 0
                    && SliceAccessPolicy::mayChange(*ownership, station, id)
                    && !ownership->mark(id).isHeld();
            };
            const int rx = ownership->activeRxFor(station);
            if (controlled(rx)) { return model.sliceById(rx); }
            const int chosen = ownership->activeFor(station);
            if (controlled(chosen)) { return model.sliceById(chosen); }
            for (SliceModel* slice : model.slices()) {
                if (slice && controlled(slice->sliceIndex())) { return slice; }
            }
            return nullptr;
        };
        LongpathContent::Sources sources;
        sources.radio = &model;
        sources.sliceForContext = stationSlice;
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, sources);
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyCustomType));
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* freq = qobject_cast<FrequencyInstrument*>(widget.get());
        QVERIFY(freq);
        QCOMPARE(freq->frequency(), 0.0);

        // Connected after the instrument, as the station policy is.
        connect(&model, &RadioModel::sliceAdded, &model, [&model, station](int) {
            model.sliceOwnership()->adoptUnowned(station);
        });
        model.addSlice();
        QCOMPARE(model.slices().size(), 1);
        SliceModel* slice = model.slices().at(0);
        QCOMPARE(model.sliceOwnership()->mark(slice->sliceIndex()).owner, station);
        QCOMPARE(freq->frequency(), 14225000.0);

        slice->setFrequency(7240000.0);
        QCOMPARE(freq->frequency(), 7240000.0);
    }

    void frequencyShowsWhenRemoteSliceAccessArrives()
    {
        // A remote window: the snapshot's slice arrives first, and the
        // window finds its slice only once the Core's slice access entry
        // for it is in (SliceAccessMirror::changed). No RadioModel or
        // SliceOwnership signal fires then, so the instrument follows the
        // access change through Sources::watchSliceAccess.
        RadioModel model;
        model.addSlice();
        QCOMPARE(model.slices().size(), 1);
        SliceModel* slice = model.slices().at(0);
        bool accessIn = false;
        QObject access;  // objectNameChanged stands in for SliceAccessMirror::changed
        LongpathContent::Sources sources;
        sources.radio = &model;
        sources.sliceForContext = [&accessIn, slice](const QJsonObject&) -> SliceModel* {
            return accessIn ? slice : nullptr;
        };
        sources.watchSliceAccess = [&access](QObject* context, std::function<void()> changed) {
            QObject::connect(&access, &QObject::objectNameChanged, context,
                             [changed = std::move(changed)](const QString&) { changed(); });
        };
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, sources);
        const ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyCustomType));
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* freq = qobject_cast<FrequencyInstrument*>(widget.get());
        QVERIFY(freq);
        QCOMPARE(freq->frequency(), 0.0);

        accessIn = true;
        access.setObjectName(QStringLiteral("entry for slice 0"));
        QCOMPARE(freq->frequency(), 14225000.0);
        slice->setFrequency(7240000.0);
        QCOMPARE(freq->frequency(), 7240000.0);
    }

    void barDefaultsAndReadings()
    {
        ContainerContentRegistry registry;
        MeterPoller poller;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kBarType));
        entry.context.insert(QStringLiteral("sliceId"), 0);
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* bar = qobject_cast<BarInstrument*>(widget.get());
        QVERIFY(bar);
        QVERIFY(bar->isCompact());
        QCOMPARE(bar->primary(), int(MeterBinding::TxSwr));
        QVERIFY(!bar->hasValue());
        // Another context's reading is not this bar's.
        emit poller.readingUpdated(QJsonObject{{"sliceId", 1}}, MeterBinding::TxSwr, 1.25);
        QVERIFY(!bar->hasValue());
        emit poller.readingUpdated(entry.context, MeterBinding::TxSwr, 1.25);
        QVERIFY(bar->hasValue());
        emit poller.readingUpdated(entry.context, MeterBinding::TxSwr, kNoMeterReadingDbm);
        QVERIFY(!bar->hasValue());
    }

    void noMeterReadingClearsTheSecondary()
    {
        // A no-meter reading on the secondary binding drops the secondary
        // value, as one on the primary drops the primary.
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);
        for (const char* type : {LongpathContent::kNeedleType, LongpathContent::kBarType}) {
            ContentEntry entry = registry.makeEntry(QString::fromLatin1(type));
            const int primary = std::strcmp(type, LongpathContent::kBarType) == 0
                                    ? int(MeterBinding::TxSwr) : int(MeterBinding::SignalPeak);
            const int secondary = std::strcmp(type, LongpathContent::kBarType) == 0
                                      ? int(MeterBinding::TxPower) : int(MeterBinding::SignalAvg);
            entry.config.insert(QString::fromLatin1(LongpathContent::kSecondaryKey), secondary);
            entry.context = QJsonObject{{QStringLiteral("rxSource"), 1}};
            QWidget parent;
            std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
            auto* needle = qobject_cast<NeedleInstrument*>(widget.get());
            auto* bar = qobject_cast<BarInstrument*>(widget.get());
            QVERIFY(needle || bar);
            const auto hasSecond = [&] { return needle ? needle->hasSecondValue() : bar->hasSecondValue(); };
            const auto hasPrimary = [&] { return needle ? needle->hasValue() : bar->hasValue(); };
            emit poller.readingUpdated(entry.context, primary, primary == MeterBinding::TxSwr ? 1.5 : -80.0);
            emit poller.readingUpdated(entry.context, secondary, secondary == MeterBinding::TxPower ? 50.0 : -85.0);
            QVERIFY(hasPrimary());
            QVERIFY(hasSecond());
            emit poller.readingUpdated(entry.context, secondary, kNoMeterReadingDbm);
            QVERIFY(!hasSecond());
            QVERIFY(hasPrimary());
        }
    }

    void needleReadsItsSliceWithoutAMeterWidget()
    {
        // Slice B has no MeterWidget target: only the needle asks for it.
        MeterPoller poller;
        QList<QJsonObject> polled;
        poller.setRxReadingSource([&](const QJsonObject& context, int) {
            polled.append(context);
            return context.value(QStringLiteral("rxSource")).toInt() == 2 ? -73.0 : -93.0;
        });
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        const QJsonObject sliceB{{QStringLiteral("rxSource"), 2}};
        entry.context = sliceB;
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* needle = qobject_cast<NeedleInstrument*>(widget.get());
        QVERIFY(needle);
        QCOMPARE(poller.targetCountForTest(), 0);
        QCOMPARE(poller.readingConsumerCountForTest(), 1);

        // The poller's own frame produces slice B's reading for the needle.
        QList<double> delivered;
        connect(&poller, &MeterPoller::readingUpdated, this, [&](const QJsonObject& context, int binding, double value) {
            if (context == sliceB && binding == MeterBinding::SignalPeak) { delivered.append(value); }
        });
        polled.clear();
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QVERIFY(polled.contains(sliceB));
        QCOMPARE(delivered, (QList<double>{-73.0}));
        QVERIFY(needle->hasValue());
        QVERIFY(!needle->isOffline());

        // Destroying the needle drops its context from the poll set.
        widget.reset();
        QCOMPARE(poller.readingConsumerCountForTest(), 0);
        polled.clear();
        delivered.clear();
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QVERIFY(!polled.contains(sliceB));
        QVERIFY(delivered.isEmpty());
    }

    void reRegisteringMovesTheContext()
    {
        MeterPoller poller;
        QList<QJsonObject> polled;
        poller.setRxReadingSource([&](const QJsonObject& context, int) { polled.append(context); return -80.0; });
        const QJsonObject a{{QStringLiteral("rxSource"), 1}};
        const QJsonObject b{{QStringLiteral("rxSource"), 2}};
        QObject owner;
        poller.addReadingConsumer(&owner, a, {MeterBinding::SignalPeak});
        poller.addReadingConsumer(&owner, b, {MeterBinding::SignalPeak});
        QCOMPARE(poller.readingConsumerCountForTest(), 1);
        polled.clear();
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(polled, (QList<QJsonObject>{b}));
        poller.removeReadingConsumer(&owner);
        polled.clear();
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QVERIFY(polled.isEmpty());
    }

    void sourceChangeMovesTheRegistration()
    {
        MeterPoller poller;
        QList<QJsonObject> polled;
        poller.setRxReadingSource([&](const QJsonObject& context, int) { polled.append(context); return -80.0; });
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);

        QTemporaryDir directory;
        AppSettings settings{directory.filePath("longpath-source.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        entry.id = QStringLiteral("station-needle");
        entry.context = QJsonObject{{QStringLiteral("rxSource"), 1}};
        ContainerDocument c;
        c.id = QStringLiteral("station");
        c.name = QStringLiteral("Station");
        c.layout = ContentLayout::VerticalStack;
        c.dockMode = DockMode::OverlayDocked;
        c.geometry = QRect(10, 10, 380, 700);
        c.contents = {entry};
        WorkspaceDocument document;
        document.containers.append(c);
        document.mainContainerId = c.id;
        auto committed = manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        auto* host = manager.contentHost(c.id);
        QVERIFY(host);
        auto needles = host->findChildren<NeedleInstrument*>();
        QCOMPARE(needles.size(), 1);
        const QJsonObject before = needles.first()->property("containerSourceContext").toJsonObject();
        QCOMPARE(poller.readingConsumerCountForTest(), 1);

        document.containers[0].contents[0].context = QJsonObject{{QStringLiteral("rxSource"), 2}};
        committed = manager.commitWorkspace(document, committed.revision);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        settle();
        host = manager.contentHost(c.id);
        QVERIFY(host);
        needles = host->findChildren<NeedleInstrument*>();
        QCOMPARE(needles.size(), 1);
        const QJsonObject after = needles.first()->property("containerSourceContext").toJsonObject();
        QVERIFY(after != before);
        QCOMPARE(poller.readingConsumerCountForTest(), 1);
        polled.clear();
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QVERIFY(polled.contains(after));
        QVERIFY(!polled.contains(before));
    }

    void sliceSwitchDropsTheOldPeak()
    {
        // A follow-selected context: the slice behind it is the selected one.
        MeterPoller poller;
        QByteArray selected("slice-a");
        poller.setRxSourceIdentitySource([&](const QJsonObject&) { return selected; });
        poller.setRxReadingSource([&](const QJsonObject&, int) {
            return selected == "slice-a" ? -60.0 : -90.0;
        });
        const QJsonObject followsSelected{{QStringLiteral("rxSource"), 0}};
        // Connected before the needle exists, so it runs before the needle
        // takes each reading.
        bool checkedFirstB = false;
        NeedleInstrument* needle = nullptr;
        connect(&poller, &MeterPoller::readingUpdated, this, [&](const QJsonObject& context, int binding, double value) {
            if (!needle || checkedFirstB || context != followsSelected || binding != MeterBinding::SignalPeak || value != -90.0) { return; }
            checkedFirstB = true;
            QVERIFY(!needle->hasValue());
            QVERIFY(needle->peakHold().value() != -60.0);
        });
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        entry.context = followsSelected;
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        needle = qobject_cast<NeedleInstrument*>(widget.get());
        QVERIFY(needle);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(needle->peakHold().value(), -60.0);

        selected = "slice-b";
        QSignalSpy reset(&poller, &MeterPoller::rxSourceChanged);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(reset.count(), 1);
        QVERIFY(checkedFirstB);
        QVERIFY(needle->hasValue());
        QCOMPARE(needle->peakHold().value(), -90.0);
        // The same slice again: no further reset.
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(reset.count(), 1);
    }

    void unavailableReadingShowsNoValue()
    {
        // No transmit source: the poller names the reason, as a MeterWidget
        // shows the ALC binding disabled, and the bar shows no value.
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kBarType));
        entry.config.insert(QString::fromLatin1(LongpathContent::kBindingKey), MeterBinding::TxAlc);
        entry.context = QJsonObject{{QStringLiteral("rxSource"), 2}};
        QWidget parent;
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* bar = qobject_cast<BarInstrument*>(widget.get());
        QVERIFY(bar);
        emit poller.readingUpdated(entry.context, MeterBinding::TxAlc, -3.0);
        QVERIFY(bar->hasValue());
        poller.setTxChannel(nullptr);
        QVERIFY(!poller.bindingUnavailableReason(MeterBinding::TxAlc).isEmpty());
        QVERIFY(!bar->hasValue());
        emit poller.readingUpdated(entry.context, MeterBinding::TxAlc, -3.0);
        QVERIFY(!bar->hasValue());
    }

    void previewNeedleIsInertAndSeeded()
    {
        // Container Settings preview: the needle shows the cached reading
        // for the entry's effective context (slice B from the container
        // default) and registers nothing with the poller.
        MeterPoller poller;
        QJsonObject effective;
        poller.setRxReadingSource([&effective](const QJsonObject& context, int) {
            return context == effective ? -73.0 : -93.0;
        });
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);

        ContainerDocument document;
        document.id = QStringLiteral("preview-container");
        document.layout = ContentLayout::VerticalStack;
        document.config.insert(QStringLiteral("rxSource"), 2);
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        entry.id = QStringLiteral("preview-needle");
        entry.context = {};
        document.contents.append(entry);
        effective = ContainerContentHost::effectiveContext(document, entry);
        QVERIFY(effective != entry.context);

        ContainerPreviewWidget preview(registry, poller);
        preview.setDocument(document);
        const auto needles = preview.findChildren<NeedleInstrument*>();
        QCOMPARE(needles.size(), 1);
        NeedleInstrument* needle = needles.constFirst();
        QCOMPARE(poller.readingConsumerCountForTest(), 0);
        QVERIFY(needle->hasValue());
        QCOMPARE(needle->peakHold().value(), -73.0);
        QVERIFY(!needle->isOffline());

        // Inert: a live reading for its context does not move it.
        emit poller.readingUpdated(effective, MeterBinding::SignalPeak, -40.0);
        QCOMPARE(needle->peakHold().value(), -73.0);

        // The frequency and filter previews follow no slice and run no timer.
        for (const char* type : {LongpathContent::kFrequencyType, LongpathContent::kFrequencyCustomType,
                                 LongpathContent::kFilterType}) {
            const ContentEntry other = registry.makeEntry(QString::fromLatin1(type));
            QWidget parent;
            std::unique_ptr<QWidget> view(registry.createPreview(other, &parent, document.config));
            QVERIFY(view);
            QVERIFY(view->findChildren<QTimer*>().isEmpty());
            for (QObject* child : view->findChildren<QObject*>(Qt::FindDirectChildrenOnly)) {
                QVERIFY2(std::strcmp(child->metaObject()->className(), "QObject") != 0,
                         "a preview holds no slice follower");
            }
        }
    }

    void registrationReplayReachesOnlyTheNewWidget()
    {
        // Two TX power needles on one context. Building the second replays
        // the cache (no TX power reading: none) to it alone; the first
        // keeps its value.
        MeterPoller poller;
        ContainerContentRegistry registry;
        LongpathContent::Sources sources;
        sources.poller = &poller;
        LongpathContent::registerAll(registry, sources);
        ContentEntry entry = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        entry.config.insert(QString::fromLatin1(LongpathContent::kDirectionKey), QStringLiteral("tx"));
        entry.context = QJsonObject{{QStringLiteral("rxSource"), 1}};
        QWidget parent;
        std::unique_ptr<QWidget> first(registry.createWidget(entry, &parent));
        auto* held = qobject_cast<NeedleInstrument*>(first.get());
        QVERIFY(held);
        QCOMPARE(held->primary(), int(MeterBinding::TxPower));
        emit poller.readingUpdated(entry.context, MeterBinding::TxPower, 50.0);
        QVERIFY(held->hasValue());

        QSignalSpy shared(&poller, &MeterPoller::readingUpdated);
        std::unique_ptr<QWidget> second(registry.createWidget(entry, &parent));
        auto* fresh = qobject_cast<NeedleInstrument*>(second.get());
        QVERIFY(fresh);
        QCOMPARE(shared.count(), 0);
        QVERIFY(held->hasValue());
        QVERIFY(!fresh->hasValue());
        QCOMPARE(poller.readingConsumerCountForTest(), 2);

        // An RX needle's replay still seeds the new widget.
        poller.setRxReadingSource([](const QJsonObject&, int) { return -81.0; });
        shared.clear();
        ContentEntry rx = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        rx.context = entry.context;
        std::unique_ptr<QWidget> third(registry.createWidget(rx, &parent));
        auto* seeded = qobject_cast<NeedleInstrument*>(third.get());
        QVERIFY(seeded);
        QVERIFY(seeded->hasValue());
        QCOMPARE(seeded->peakHold().value(), -81.0);
        QCOMPARE(shared.count(), 0);
    }

    void editorShowsDeclaredProperties()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        ContentPropertyEditor editor(registry);
        QSignalSpy edited(&editor, &ContentPropertyEditor::entryEdited);

        // Needle: the reading bindings ReadingSource offers; the two with no
        // NereusSDR producer are listed but disabled.
        editor.setEntry(registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType)));
        auto* binding = editor.findChild<QComboBox*>(QStringLiteral("property_binding"));
        QVERIFY(binding);
        QCOMPARE(binding->currentData().toInt(), int(MeterBinding::SignalPeak));
        QVERIFY(binding->isEnabled());
        auto* model = qobject_cast<QStandardItemModel*>(binding->model());
        QVERIFY(model);
        for (int id : {kNoiseFloorReading, kRadeSnrReading}) {
            const int row = binding->findData(id);
            QVERIFY(row >= 0);
            QVERIFY(!model->item(row)->isEnabled());
        }
        QVERIFY(model->item(binding->findData(int(MeterBinding::TxSwr)))->isEnabled());
        auto* secondary = editor.findChild<QComboBox*>(QStringLiteral("property_secondaryBinding"));
        QVERIFY(secondary);
        QVERIFY(secondary->currentData().isNull());
        for (const QLabel* l : editor.findChildren<QLabel*>()) {
            QVERIFY(!l->text().startsWith(QStringLiteral("Unsupported configuration")));
        }
        binding->setCurrentIndex(binding->findData(int(MeterBinding::TxSwr)));
        QCOMPARE(edited.size(), 1);
        auto entry = edited.takeLast().at(0).value<ContentEntry>();
        QCOMPARE(entry.config.value(QString::fromLatin1(LongpathContent::kBindingKey)).toInt(),
                 int(MeterBinding::TxSwr));

        // Frequency form.
        editor.setEntry(registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyType)));
        auto* form = editor.findChild<QComboBox*>(QStringLiteral("property_form"));
        QVERIFY(form);
        QCOMPARE(form->currentData().toString(), QStringLiteral("numberOnly"));
        QCOMPARE(form->count(), 3);
        edited.clear();
        form->setCurrentIndex(form->findData(QStringLiteral("bandStrip")));
        QCOMPARE(edited.size(), 1);
        entry = edited.takeLast().at(0).value<ContentEntry>();
        QCOMPARE(entry.config.value(QString::fromLatin1(LongpathContent::kFormKey)).toString(),
                 QStringLiteral("bandStrip"));

        // Bar compact, on by default.
        editor.setEntry(registry.makeEntry(QString::fromLatin1(LongpathContent::kBarType)));
        auto* compact = editor.findChild<QCheckBox*>(QStringLiteral("property_compact"));
        QVERIFY(compact);
        QVERIFY(compact->isChecked());
        edited.clear();
        compact->setChecked(false);
        QCOMPARE(edited.size(), 1);
        entry = edited.takeLast().at(0).value<ContentEntry>();
        QCOMPARE(entry.config.value(QString::fromLatin1(LongpathContent::kCompactKey)), QJsonValue(false));

        // Filter label; empty removes the key.
        editor.setEntry(registry.makeEntry(QString::fromLatin1(LongpathContent::kFilterType)));
        auto* label = editor.findChild<QLineEdit*>(QStringLiteral("property_label"));
        QVERIFY(label);
        QVERIFY(label->text().isEmpty());
        edited.clear();
        label->setText(QStringLiteral("Main"));
        emit label->editingFinished();
        QCOMPARE(edited.size(), 1);
        entry = edited.takeLast().at(0).value<ContentEntry>();
        QCOMPARE(entry.config.value(QString::fromLatin1(LongpathContent::kLabelKey)).toString(), QStringLiteral("Main"));
        label->clear();
        emit label->editingFinished();
        entry = edited.takeLast().at(0).value<ContentEntry>();
        QVERIFY(!entry.config.contains(QString::fromLatin1(LongpathContent::kLabelKey)));
        QVERIFY(editor.findChild<QGroupBox*>(QStringLiteral("declaredProperties")));

        // A needle declares only readings: no empty appearance group.
        editor.setEntry(registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType)));
        QVERIFY(!editor.findChild<QGroupBox*>(QStringLiteral("declaredProperties")));
        const auto advancedGroup = [&editor]() -> QGroupBox* {
            for (QGroupBox* g : editor.findChildren<QGroupBox*>()) {
                if (g->title() == QStringLiteral("Advanced bindings")) { return g; }
            }
            return nullptr;
        };
        QVERIFY(advancedGroup() && advancedGroup()->isEnabled());

        // An unavailable declared type: Advanced bindings disabled, not hidden.
        ContentEntry unavailable = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        unavailable.extensions.insert(QStringLiteral("unavailableReason"), QStringLiteral("Not in this build."));
        editor.setEntry(unavailable);
        QGroupBox* advanced = advancedGroup();
        QVERIFY(advanced);
        QVERIFY(!advanced->isEnabled());
        QVERIFY(!advanced->isHidden());
        QCOMPARE(advanced->toolTip(), QStringLiteral("Not in this build."));
    }

    void customFrequencyEditorWritesDisplayConfig()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        ContentPropertyEditor editor(registry);
        QSignalSpy edited(&editor, &ContentPropertyEditor::entryEdited);

        editor.setEntry(registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyCustomType)));
        auto* form = editor.findChild<QComboBox*>(QStringLiteral("property_form"));
        QVERIFY(form);
        QCOMPARE(form->count(), 3);
        auto* size = editor.findChild<QSpinBox*>(QStringLiteral("property_fontSize"));
        QVERIFY(size);
        QCOMPARE(size->value(), 26);
        QCOMPARE(size->minimum(), 12);
        QCOMPARE(size->maximum(), 160);
        auto* fit = editor.findChild<QCheckBox*>(QStringLiteral("property_fitWidth"));
        QVERIFY(fit);
        QVERIFY(!fit->isChecked());
        auto* color = editor.findChild<QPushButton*>(QStringLiteral("property_fontColor"));
        QVERIFY(color);
        QCOMPARE(color->text(), QStringLiteral("#d8a55f"));
        auto* center = editor.findChild<QCheckBox*>(QStringLiteral("property_center"));
        QVERIFY(center);
        QVERIFY(!center->isChecked());
        auto* own = editor.findChild<QCheckBox*>(QStringLiteral("property_showOwnSlice"));
        auto* other = editor.findChild<QCheckBox*>(QStringLiteral("property_showOtherSlice"));
        QVERIFY(own && other);
        QVERIFY(own->isChecked());
        QVERIFY(other->isChecked());

        edited.clear();
        size->setValue(48);
        QCOMPARE(edited.size(), 1);
        auto entry = edited.takeLast().at(0).value<ContentEntry>();
        QCOMPARE(entry.config.value(QString::fromLatin1(LongpathContent::kFontSizeKey)).toInt(), 48);
        fit->setChecked(true);
        entry = edited.takeLast().at(0).value<ContentEntry>();
        QCOMPARE(entry.config.value(QString::fromLatin1(LongpathContent::kFitWidthKey)), QJsonValue(true));
        center->setChecked(true);
        entry = edited.takeLast().at(0).value<ContentEntry>();
        QCOMPARE(entry.config.value(QString::fromLatin1(LongpathContent::kCenterKey)), QJsonValue(true));
        own->setChecked(false);
        entry = edited.takeLast().at(0).value<ContentEntry>();
        QCOMPARE(entry.config.value(QString::fromLatin1(LongpathContent::kShowOwnKey)), QJsonValue(false));

        // The original object keeps its single form control.
        editor.setEntry(registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyType)));
        QVERIFY(editor.findChild<QComboBox*>(QStringLiteral("property_form")));
        QVERIFY(!editor.findChild<QSpinBox*>(QStringLiteral("property_fontSize")));
        QVERIFY(!editor.findChild<QCheckBox*>(QStringLiteral("property_center")));
    }

    void customFrequencyHonorsFontColorCenterAndSliceLines()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        QWidget parent;

        ContentEntry plain = registry.makeEntry(QString::fromLatin1(LongpathContent::kFrequencyCustomType));
        std::unique_ptr<QWidget> plainWidget(registry.createWidget(plain, &parent));
        auto* stock = qobject_cast<FrequencyInstrument*>(plainWidget.get());
        QVERIFY(stock);
        QVERIFY(stock->digitFontLocked());
        QCOMPARE(stock->digitFontPx(), 26);
        QVERIFY(!stock->contentCentered());
        QCOMPARE(stock->sizePolicy().verticalPolicy(), QSizePolicy::Maximum);
        QVERIFY(stock->ownSliceLineVisible());
        QVERIFY(stock->otherSliceLineVisible());
        // isVisible() is false while the parent is hidden, even when the
        // slice line was not turned off.
        parent.resize(800, 600);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));
        settle();
        QVERIFY(stock->vfoRowVisible());
        QVERIFY(!stock->digitColor().isValid());

        ContentEntry entry = plain;
        entry.config.insert(QString::fromLatin1(LongpathContent::kFontSizeKey), 36);
        entry.config.insert(QString::fromLatin1(LongpathContent::kFitWidthKey), false);
        entry.config.insert(QString::fromLatin1(LongpathContent::kFontColorKey), QStringLiteral("#44aaff"));
        entry.config.insert(QString::fromLatin1(LongpathContent::kCenterKey), true);
        entry.config.insert(QString::fromLatin1(LongpathContent::kShowOwnKey), false);
        std::unique_ptr<QWidget> widget(registry.createWidget(entry, &parent));
        auto* freq = qobject_cast<FrequencyInstrument*>(widget.get());
        QVERIFY(freq);
        freq->setFrequency(14200000.0);
        freq->setOtherFrequency(7185000.0);
        QCOMPARE(freq->digitFontPx(), 36);
        QVERIFY(freq->digitFontLocked());
        QCOMPARE(freq->digitColor(), QColor(QStringLiteral("#44aaff")));
        QVERIFY(freq->contentCentered());
        QCOMPARE(freq->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
        QVERIFY(!freq->ownSliceLineVisible());
        QVERIFY(freq->otherSliceLineVisible());
        freq->show();
        settle();
        QVERIFY(freq->vfoRowVisible());

        QLabel* digit = nullptr;
        QString row;
        for (QLabel* label : freq->findChildren<QLabel*>()) {
            if (!digit && label->text() == QLatin1String("1")) {
                digit = label;
            }
            if (label->text().contains(QLatin1String("<font"))) {
                row = label->text();
            }
        }
        QVERIFY(digit);
        QCOMPARE(digit->font().pixelSize(), 36);
        QVERIFY(digit->styleSheet().contains(QStringLiteral("#44aaff")));
        QVERIFY2(row.contains(QStringLiteral(">B ")), qPrintable(row));
        QVERIFY2(!row.contains(QStringLiteral(">A ")), qPrintable(row));

        freq->setParent(nullptr);
        freq->resize(720, 260);
        freq->show();
        QVERIFY(QTest::qWaitForWindowExposed(freq));
        settle();
        const QPoint origin = digit->mapTo(freq, QPoint(0, 0));
        QVERIFY2(origin.x() > 60, qPrintable(QString::number(origin.x())));
        QVERIFY2(origin.y() > 30, qPrintable(QString::number(origin.y())));

        ContentEntry hidden = plain;
        hidden.config.insert(QString::fromLatin1(LongpathContent::kShowOwnKey), false);
        hidden.config.insert(QString::fromLatin1(LongpathContent::kShowOtherKey), false);
        std::unique_ptr<QWidget> hiddenWidget(registry.createWidget(hidden, &parent));
        auto* bothOff = qobject_cast<FrequencyInstrument*>(hiddenWidget.get());
        QVERIFY(bothOff);
        bothOff->show();
        settle();
        QVERIFY(!bothOff->vfoRowVisible());
        QVERIFY(bothOff->minimumSizeHint().height() < stock->minimumSizeHint().height());

        ContentEntry fitting = plain;
        fitting.config.insert(QString::fromLatin1(LongpathContent::kFontSizeKey), 48);
        fitting.config.insert(QString::fromLatin1(LongpathContent::kFitWidthKey), true);
        std::unique_ptr<QWidget> fitWidget(registry.createWidget(fitting, &parent));
        auto* fitted = qobject_cast<FrequencyInstrument*>(fitWidget.get());
        QVERIFY(fitted);
        QVERIFY(!fitted->digitFontLocked());
        fitted->setParent(nullptr);
        fitted->resize(180, 120);
        fitted->show();
        QVERIFY(QTest::qWaitForWindowExposed(fitted));
        settle();
        QVERIFY(fitted->digitFontPx() < 48);
        QVERIFY(fitted->digitFontPx() >= 12);
    }

    void compactAndLabelReachTheWidgets()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        QWidget parent;
        ContentEntry barEntry = registry.makeEntry(QString::fromLatin1(LongpathContent::kBarType));
        barEntry.config.insert(QString::fromLatin1(LongpathContent::kCompactKey), false);
        std::unique_ptr<QWidget> bar(registry.createWidget(barEntry, &parent));
        QVERIFY(qobject_cast<BarInstrument*>(bar.get()));
        QVERIFY(!qobject_cast<BarInstrument*>(bar.get())->isCompact());

        ContentEntry filterEntry = registry.makeEntry(QString::fromLatin1(LongpathContent::kFilterType));
        filterEntry.config.insert(QString::fromLatin1(LongpathContent::kLabelKey), QStringLiteral("Main"));
        std::unique_ptr<QWidget> pane(registry.createWidget(filterEntry, &parent));
        settle();
        QVERIFY(qobject_cast<BandwidthFilterPane*>(pane.get()));
        QCOMPARE(qobject_cast<BandwidthFilterPane*>(pane.get())->label(), QStringLiteral("Main"));
    }

    void sourceMenuCommitsTheContainerSource()
    {
        ContainerContentRegistry registry;
        LongpathContent::registerAll(registry, {});
        QTemporaryDir directory;
        AppSettings settings{directory.filePath("source.xml")};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        dock.resize(900, 900);
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        // Slices A and B are open.
        manager.setSliceAvailability([](int sliceId) { return sliceId >= 0 && sliceId < 2; });
        ContainerDocument c;
        c.id = QStringLiteral("meters");
        c.name = QStringLiteral("Meters");
        c.layout = ContentLayout::VerticalStack;
        c.dockMode = DockMode::OverlayDocked;
        c.geometry = QRect(10, 10, 380, 300);
        c.config.insert(QStringLiteral("rxSource"), 1);
        ContentEntry needle = registry.makeEntry(QString::fromLatin1(LongpathContent::kNeedleType));
        needle.id = QStringLiteral("meters-needle");
        c.contents = {needle};
        WorkspaceDocument document;
        document.containers.append(c);
        document.mainContainerId = c.id;
        QVERIFY(manager.commitWorkspace(document, 0).status == CommitStatus::Saved);
        settle();
        ContainerWidget* widget = manager.container(c.id);
        QVERIFY(widget && widget->workspaceHeader());
        QCOMPARE(widget->workspaceHeader()->sourceText(), QStringLiteral("RX A"));

        const auto config = [&store] { return store.snapshot().containers.constFirst().config; };
        const auto menuActions = [widget] {
            auto menu = std::make_unique<QMenu>();
            widget->populateSourceMenu(*menu);
            QList<QAction*> actions;
            for (QAction* a : menu->actions()) {
                if (!a->isSeparator()) { actions.append(a); }
            }
            return std::make_pair(std::move(menu), actions);
        };
        {
            auto [menu, actions] = menuActions();
            QCOMPARE(actions.size(), 5);
            QStringList texts;
            for (QAction* a : actions) { texts.append(a->text()); }
            QCOMPARE(texts, (QStringList{QStringLiteral("RX A"), QStringLiteral("RX B"), QStringLiteral("RX C"),
                                         QStringLiteral("RX D"), QStringLiteral("Active slice")}));
            QVERIFY(actions.at(0)->isEnabled() && actions.at(0)->isChecked());
            QVERIFY(actions.at(1)->isEnabled() && !actions.at(1)->isChecked());
            // Slices that do not exist are disabled, not hidden.
            QVERIFY(!actions.at(2)->isEnabled());
            QVERIFY(!actions.at(3)->isEnabled());
            QVERIFY(actions.at(4)->isEnabled() && !actions.at(4)->isChecked());
            actions.at(1)->trigger();
        }
        settle();
        QCOMPARE(config().value("rxSource").toInt(), 2);
        QVERIFY(!config().contains("rxSourceMode"));
        QCOMPARE(widget->workspaceHeader()->sourceText(), QStringLiteral("RX B"));

        // Active slice follows the selected receiver and keeps the slice.
        {
            auto [menu, actions] = menuActions();
            QVERIFY(actions.at(1)->isChecked());
            actions.at(4)->trigger();
        }
        settle();
        QCOMPARE(config().value("rxSourceMode").toString(), QStringLiteral("followSelectedRx"));
        QCOMPARE(config().value("rxSource").toInt(), 2);
        QCOMPARE(widget->workspaceHeader()->sourceText(), QStringLiteral("Active"));
        {
            auto [menu, actions] = menuActions();
            QVERIFY(actions.at(4)->isChecked());
            QVERIFY(!actions.at(1)->isChecked());
            actions.at(0)->trigger();
        }
        settle();
        QVERIFY(!config().contains("rxSourceMode"));
        QCOMPARE(config().value("rxSource").toInt(), 1);
        QCOMPARE(widget->workspaceHeader()->sourceText(), QStringLiteral("RX A"));

        // A saved sliceId keeps its key, as Container Settings writes it.
        QVERIFY(manager.commitContainerSource(c.id, 2));
        WorkspaceDocument withSliceId = store.snapshot();
        withSliceId.containers.first().config.remove("rxSource");
        withSliceId.containers.first().config.insert("sliceId", 0);
        QVERIFY(manager.commitWorkspace(withSliceId, withSliceId.revision).status == CommitStatus::Saved);
        QVERIFY(manager.commitContainerSource(c.id, 2));
        QCOMPARE(config().value("sliceId").toInt(), 1);
        QVERIFY(!config().contains("rxSource"));
        settle();
        // The menu ticks the slice the chip shows (sliceId), not a stale
        // rxSource.
        QCOMPARE(widget->workspaceHeader()->sourceText(), QStringLiteral("RX B"));
        {
            auto [menu, actions] = menuActions();
            QVERIFY(!actions.at(0)->isChecked());
            QVERIFY(actions.at(1)->isChecked());
        }

        // A click on the chip opens the menu.
        dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dock));
        settle();
        WorkspaceSourceChip* chip = widget->workspaceHeader()->sourceChip();
        QVERIFY(chip && chip->isVisible());
        QStringList shown;
        QTimer::singleShot(0, [&shown] {
            if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                for (QAction* a : menu->actions()) { shown.append(a->text()); }
                menu->close();
            }
        });
        QTest::mouseClick(chip, Qt::LeftButton);
        QVERIFY(shown.contains(QStringLiteral("Active slice")));
    }

    void traceFromRenderedTakesBucketMaxima()
    {
        // 100 pixels over 10 kHz centred on 14.200 MHz: 100 Hz per pixel.
        QVector<float> pixels(100, -120.0f);
        pixels[55] = -60.0f;   // 14.200500 .. 14.200600 MHz
        const auto trace = LongpathContent::traceFromRendered(pixels, 14200000.0, 10000.0,
                                                              14200000.0, 14201000.0, 5);
        QCOMPARE(trace.size(), 5);
        QCOMPARE(trace.at(2), -60.0f);
        QCOMPARE(trace.at(0), -120.0f);
        // Off the pan: no trace rather than a partial one.
        QVERIFY(LongpathContent::traceFromRendered(pixels, 14200000.0, 10000.0,
                                                   14190000.0, 14201000.0, 5).isEmpty());
    }
};

QTEST_MAIN(TstLongpathInstruments)
#include "tst_longpath_instruments.moc"
