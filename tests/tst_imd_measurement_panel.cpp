// no-port-check: NereusSDR-original GUI contracts and synthetic presentation fixtures.
// Modification history (NereusSDR):
//   2026-10-26 — Skip the GPU frame cases where QRhi is unavailable by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QDir>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <memory>
#include "gui/MainWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PsForm.h"
#include "models/SliceModel.h"
#include "gui/ImdMeasurementPanel.h"
#include "gui/PureSignalViewStyle.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "gui/PanadapterApplet.h"
#include "gui/MoxDisplayController.h"
#include "gui/SpectrumWidget.h"
#include "gui/styles/WorkspacePreview.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/TwoToneController.h"
#include "models/RadioModel.h"
#include "support/ImdSpectrumTestAccess.h"
#include "NativeWindowFrames.h"
namespace NereusSDR {
class ImdPanelTestAccess {
public:
    static void prepare(MainWindow& window) {
        if (!window.m_moxDisplay) { window.m_moxDisplay = new MoxDisplayController(window.m_panStack, window.m_radioModel, &window); }
        if (!window.m_psForm) { window.m_psForm = new PsForm(window.m_radioModel, nullptr, &window); }
        window.wireImdMeasurementForwarding();
    }
    static PanadapterStack* pans(MainWindow& window) { return window.m_panStack; }
    static RadioModel* radio(MainWindow& window) { return window.m_radioModel; }
    static MoxDisplayController* controller(MainWindow& window) { return window.m_moxDisplay; }
    static PsForm* form(MainWindow& window) { return window.m_psForm; }
};
}
using namespace NereusSDR;
namespace {
QVector<float> pixels(int count) {
    QVector<float> result(count, -120.0f);
    const std::array<float, 6> levels{-22, -24, -65, -66, -88, -89};
    const std::array<double, 6> positions{.425, .575, .275, .725, .125, .875};
    for (int i = 0; i < 6; ++i) { result[qRound(positions[i] * (count - 1))] = levels[i]; }
    return result;
}
void configure(SpectrumWidget& widget) {
    widget.setDbmScaleVisible(false);
    widget.setSampleRate(192000.0);
    widget.setDdcCenterFrequency(14200000.0);
    widget.setDisplayWindowPreservingHistory(14200000.0, 48000.0);
    widget.setDisplayDuplex(true);
    widget.setMoxOverlay(true);
    widget.setTestingIMD(true);
    widget.setShowIMDMeasurements(true);
    widget.setDbmRange(-140.0f, 0.0f);
}
QImage renderOrdinaryWidget(QWidget& widget) {
    const qreal ratio = widget.devicePixelRatioF();
    QImage image(qRound(widget.width() * ratio), qRound(widget.height() * ratio), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(ratio);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
}
int inkPixels(const QImage& image, const QColor& ink) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() > 200 && std::abs(pixel.red() - ink.red()) <= 3 && std::abs(pixel.green() - ink.green()) <= 3 && std::abs(pixel.blue() - ink.blue()) <= 3) { ++count; }
        }
    }
    return count;
}
QString textOf(QWidget& widget, const char* name) { return widget.findChild<QLabel*>(QString::fromLatin1(name))->text(); }
}
class TestImdMeasurementPanel : public QObject {
    Q_OBJECT
    QTemporaryDir m_directory;
    std::unique_ptr<AppSettings> m_settings;
private slots:
    void initTestCase() { m_settings = std::make_unique<AppSettings>(m_directory.filePath(QStringLiteral("panel.settings"))); }
    void invalidFrameExplainsReasonWithoutNumbers() {
        ImdMeasurementPanel panel;
        panel.setPresentation(ImdMeasurementPanel::Presentation::Inspector);
        for (ImdStatus status : {ImdStatus::WaitingForTx, ImdStatus::DuplexRequired, ImdStatus::WaitingForSpectrum,
                                ImdStatus::FundamentalsUnresolved, ImdStatus::ProductsOutsideView, ImdStatus::ProductsMissing}) {
            ImdMeasurementFrame frame;
            frame.status = status;
            panel.setFrame(frame);
            QCOMPARE(textOf(panel, "ImdStatus"), ImdMeasurementPanel::statusText(status));
            QVERIFY(textOf(panel, "ImdWorstThird").contains(QStringLiteral("—")));
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < 3; ++j) {
                    QCOMPARE(panel.findChild<QLabel*>(QStringLiteral("ImdReading%1_%2").arg(i).arg(j))->text(), QStringLiteral("—"));
                }
            }
        }
    }
    void railCloseRequestsMeasurementVisibilityClear()
    {
        ImdMeasurementPanel panel;
        panel.resize(736, 180);
        panel.show();
        QSignalSpy hidden(&panel, &ImdMeasurementPanel::hideMeasurementsRequested);
        QVERIFY(hidden.isValid());
        QPushButton* close = panel.findChild<QPushButton*>(QStringLiteral("ImdHideMeasurements"));
        QVERIFY(close);
        QVERIFY(close->isVisible());
        QVERIFY(close->fontMetrics().height() >= 11);
        QCOMPARE(close->accessibleName(), QStringLiteral("Hide two-tone measurements"));
        QVERIFY(close->toolTip().contains(QStringLiteral("Hide")));
        close->click();
        QCOMPARE(hidden.count(), 1);

        panel.setPresentation(ImdMeasurementPanel::Presentation::Inspector);
        QVERIFY(!close->isVisible());
        panel.setPresentation(ImdMeasurementPanel::Presentation::Window);
        QVERIFY(!close->isVisible());
    }
    void enableDupActionFollowsStatusAndCapability()
    {
        ImdMeasurementPanel panel;
        panel.resize(736, 180);
        panel.setDisplayDuplexCapability(true);
        panel.show();
        QSignalSpy requested(&panel, &ImdMeasurementPanel::enableDisplayDuplexRequested);
        QVERIFY(requested.isValid());
        QPushButton* enable = panel.findChild<QPushButton*>(QStringLiteral("ImdEnableDuplex"));
        QVERIFY(enable);
        QVERIFY(!enable->isVisible());
        QVERIFY(enable->sizePolicy().retainSizeWhenHidden());
        QVERIFY(enable->fontMetrics().height() >= 11);

        ImdMeasurementFrame frame;
        frame.status = ImdStatus::DuplexRequired;
        panel.setFrame(frame);
        QVERIFY(enable->isVisible());
        QVERIFY(enable->isEnabled());
        QCOMPARE(enable->text(), QStringLiteral("Enable DUP"));
        QVERIFY(textOf(panel, "ImdSource").contains(QStringLiteral("receiver spectrum"), Qt::CaseInsensitive));
        enable->click();
        QCOMPARE(requested.count(), 1);

        frame.status = ImdStatus::WaitingForSpectrum;
        panel.setFrame(frame);
        QVERIFY(!enable->isVisible());
        frame.status = ImdStatus::Valid;
        panel.setFrame(frame);
        QVERIFY(!enable->isVisible());
    }
    void unavailableDupReasonRemainsVisible()
    {
        const QString reason = QStringLiteral(
            "This Core does not show the receiver while transmitting for this app. Updating the Core may help.");
        ImdMeasurementPanel panel;
        panel.resize(736, 180);
        panel.setDisplayDuplexCapability(false, reason);
        ImdMeasurementFrame frame;
        frame.status = ImdStatus::DuplexRequired;
        panel.setFrame(frame);
        panel.show();
        QPushButton* enable = panel.findChild<QPushButton*>(QStringLiteral("ImdEnableDuplex"));
        QLabel* source = panel.findChild<QLabel*>(QStringLiteral("ImdSource"));
        QLabel* status = panel.findChild<QLabel*>(QStringLiteral("ImdStatus"));
        QVERIFY(enable);
        QVERIFY(source);
        QVERIFY(status);
        QVERIFY(!enable->isVisible());
        QVERIFY(!enable->isEnabled());
        QCOMPARE(enable->toolTip(), reason);
        QCOMPARE(source->text(), reason);
        QCOMPARE(source->toolTip(), reason);
        QCOMPARE(status->toolTip(), reason);

        panel.setDisplayDuplexCapability(true);
        QVERIFY(enable->isVisible());
        QVERIFY(enable->isEnabled());
        QCOMPARE(enable->toolTip(), QStringLiteral("Keep the receiver spectrum visible while transmitting"));
        QVERIFY(source->text().contains(QStringLiteral("receiver spectrum"), Qt::CaseInsensitive));
        QVERIFY(status->toolTip().isEmpty());
    }
    void hiddenDupActionKeepsTrailingControlsStable()
    {
        for (const int width : {736, 320}) {
            ImdMeasurementPanel panel;
            panel.resize(width, 260);
            panel.setDisplayDuplexCapability(true);
            ImdMeasurementFrame frame;
            frame.status = ImdStatus::WaitingForSpectrum;
            panel.setFrame(frame);
            panel.show();
            QCoreApplication::processEvents();
            QPushButton* guides = panel.findChild<QPushButton*>(QStringLiteral("ImdGuides"));
            QPushButton* inspector = panel.findChild<QPushButton*>(QStringLiteral("ImdInspector"));
            QPushButton* close = panel.findChild<QPushButton*>(QStringLiteral("ImdHideMeasurements"));
            QVERIFY(guides);
            QVERIFY(inspector);
            QVERIFY(close);
            const QRect guidesBefore = guides->geometry();
            const QRect inspectorBefore = inspector->geometry();
            const QRect closeBefore = close->geometry();

            frame.status = ImdStatus::DuplexRequired;
            panel.setFrame(frame);
            QCoreApplication::processEvents();
            QCOMPARE(guides->geometry(), guidesBefore);
            QCOMPARE(inspector->geometry(), inspectorBefore);
            QCOMPARE(close->geometry(), closeBefore);
            QVERIFY(panel.rect().contains(close->mapTo(&panel, close->rect().topRight())));
        }
    }
    void selectionHasSameFrequencyAsFrame() {
        PanadapterApplet pan(QStringLiteral("synthetic-selection"));
        pan.resize(736, 850);
        configure(*pan.spectrumWidget());
        ImdSpectrumTestAccess::seedVisiblePixels(*pan.spectrumWidget(), pixels(736));
        pan.spectrumWidget()->setImdInspectorVisible(true);
        const auto& frame = pan.spectrumWidget()->imdMeasurementFrame();
        QCOMPARE(frame.status, ImdStatus::Valid);
        for (int i = 0; i < 6; ++i) {
            pan.imdRail()->findChild<QPushButton*>(QStringLiteral("ImdProduct%1").arg(i))->click();
            QCOMPARE(pan.imdInspector()->selectedProduct(), static_cast<ImdProduct>(i));
            QVERIFY(textOf(*pan.imdInspector(), "ImdSelection").contains(QString::number(frame.peaks[i]->frequencyHz / 1e6, 'f', 6)));
        }
        pan.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pan));
        QPushButton* focused = pan.imdRail()->findChild<QPushButton*>(QStringLiteral("ImdProduct0"));
        focused->setFocus();
        QTest::keyClick(focused, Qt::Key_3);
        QTRY_COMPARE(pan.spectrumWidget()->imdSelectedProduct(), ImdProduct::ThirdLower);
        QCOMPARE(pan.imdInspector()->selectedProduct(), ImdProduct::ThirdLower);
    }
    void panelActionsDoNotTransmit() {
        RadioModel radio;
        MoxDisplayController controller(nullptr, &radio, nullptr);
        QSignalSpy mox(radio.moxController(), &MoxController::moxStateChanged);
        QSignalSpy tone(radio.twoToneController(), &TwoToneController::twoToneActiveChanged);
        QSignalSpy dup(&controller, &MoxDisplayController::displayDuplexChanged);
        PanadapterApplet pan(QStringLiteral("synthetic-actions"));
        const bool wasDuplex = controller.displayDuplex();
        for (int i = 0; i < 6; ++i) { pan.imdRail()->findChild<QPushButton*>(QStringLiteral("ImdProduct%1").arg(i))->click(); }
        pan.imdRail()->findChild<QPushButton*>(QStringLiteral("ImdGuides"))->click();
        pan.imdRail()->findChild<QPushButton*>(QStringLiteral("ImdInspector"))->click();
        pan.spectrumWidget()->setShowIMDMeasurements(true);
        pan.spectrumWidget()->setShowIMDMeasurements(false);
        QCOMPARE(mox.count(), 0);
        QCOMPARE(tone.count(), 0);
        QCOMPARE(dup.count(), 0);
        QCOMPARE(controller.displayDuplex(), wasDuplex);
    }
    void inspectorPreferenceIsPerPan() {
        AppSettings& settings = AppSettings::instance();
        const QStringList keys{QStringLiteral("PureSignalImdInspectorVisible_997"), QStringLiteral("PureSignalImdGuidesVisible_997"),
            QStringLiteral("PureSignalImdInspectorVisible_998"), QStringLiteral("PureSignalImdGuidesVisible_998")};
        QList<QVariant> values;
        QList<bool> present;
        for (const QString& key : keys) { present.append(settings.contains(key)); values.append(settings.value(key)); settings.remove(key); }
        SpectrumWidget one, two;
        one.setPanIndex(997); two.setPanIndex(998);
        one.loadSettings(); two.loadSettings();
        QVERIFY(!one.imdInspectorVisible()); QVERIFY(!two.imdInspectorVisible());
        one.setImdInspectorVisible(true); one.setImdGuidesVisible(false);
        QCOMPARE(settings.value(keys[0]).toString(), QStringLiteral("True"));
        QCOMPARE(settings.value(keys[1]).toString(), QStringLiteral("False"));
        QVERIFY(!two.imdInspectorVisible()); QVERIFY(two.imdGuidesVisible());
        SpectrumWidget restored;
        restored.setPanIndex(997); restored.loadSettings();
        QVERIFY(restored.imdInspectorVisible()); QVERIFY(!restored.imdGuidesVisible());
        for (int i = 0; i < keys.size(); ++i) {
            if (present[i]) { settings.setValue(keys[i], values[i]); } else { settings.remove(keys[i]); }
        }
    }
    void forwardingFollowsCommittedOwnerAndRetirement() {
        MainWindow window(RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred);
        ImdPanelTestAccess::prepare(window);
        PanadapterStack* pans = ImdPanelTestAccess::pans(window);
        RadioModel* radio = ImdPanelTestAccess::radio(window);
        MoxDisplayController* controller = ImdPanelTestAccess::controller(window);
        PsForm* form = ImdPanelTestAccess::form(window);
        controller->setShowIMDMeasurements(false);
        ImdPanelTestAccess::prepare(window);
        QCOMPARE(form->imdMeasurementFrame().status, ImdStatus::Disabled);
        controller->setShowIMDMeasurements(true);
        ImdPanelTestAccess::prepare(window);
        QCOMPARE(form->imdMeasurementFrame().status, ImdStatus::WaitingForTx);
        radio->configureStreamPool(5, 5, 192000);
        const int sliceId = radio->addSlice();
        QVERIFY(sliceId >= 0);
        PanadapterApplet* owner = pans->addPanadapter(QStringLiteral("measurement-owner"));
        owner->setActiveSliceIndex(sliceId);
        radio->sliceById(sliceId)->setPanKey(owner->panId());
        pans->setActivePan(QStringLiteral("pan-0"));
        QSignalSpy committed(controller, &MoxDisplayController::keyedOwnerCommitted);
        controller->setKeyed(true, sliceId);
        QCOMPARE(committed.count(), 1);
        QCOMPARE(committed.last()[0].toString(), owner->panId());
        QCOMPARE(form->imdMeasurementFrame().status, owner->spectrumWidget()->imdMeasurementFrame().status);
        configure(*owner->spectrumWidget());
        ImdSpectrumTestAccess::seedVisiblePixels(*owner->spectrumWidget(), pixels(736));
        QCOMPARE(form->imdMeasurementFrame().status, ImdStatus::Valid);
        QVERIFY(form->imdMeasurementFrame().summary);
        if (SpectrumWidget* other = pans->spectrum(QStringLiteral("pan-0"))) {
            configure(*other); ImdSpectrumTestAccess::seedVisiblePixels(*other, pixels(800));
            QCOMPARE(form->imdMeasurementFrame().context.frameSequence, owner->spectrumWidget()->imdMeasurementFrame().context.frameSequence);
            QCOMPARE(form->imdMeasurementFrame().context.sampleStepHz, owner->spectrumWidget()->imdMeasurementFrame().context.sampleStepHz);
        }
        pans->floatPanadapter(owner->panId());
        QCOMPARE(controller->keyedPanId(), owner->panId());
        pans->dockPanadapter(owner->panId());
        pans->removePanadapter(QStringLiteral("measurement-owner"));
        QVERIFY(!form->imdMeasurementFrame().summary);
        controller->setKeyed(false, sliceId);
        QCOMPARE(form->imdMeasurementFrame().status, ImdStatus::WaitingForTx);
        // Joining an already-keyed station before its slice/host exists commits no owner.
        controller->setKeyed(true, 1000000);
        QVERIFY(controller->keyedPanId().isEmpty());
        PanadapterApplet* late = pans->addPanadapter(QStringLiteral("late-owner"));
        late->setActiveSliceIndex(sliceId);
        radio->sliceById(sliceId)->setPanKey(late->panId());
        const int before = committed.count();
        controller->setKeyed(true, sliceId);
        QCOMPARE(committed.count(), before + 1);
        QCOMPARE(committed.last()[0].toString(), late->panId());
        configure(*late->spectrumWidget());
        ImdSpectrumTestAccess::seedVisiblePixels(*late->spectrumWidget(), pixels(736));
        QCOMPARE(form->imdMeasurementFrame().status, ImdStatus::Valid);
        controller->setKeyed(false, sliceId);
    }
    void renderedFixtures_data() {
        QTest::addColumn<int>("width"); QTest::addColumn<bool>("modern"); QTest::addColumn<int>("condition"); QTest::addColumn<bool>("inspector");
        for (int width : {736, 320}) {
            for (bool modern : {false, true}) {
                for (int condition = 0; condition < 4; ++condition) {
                    for (bool inspector : {false, true}) {
                        const QByteArray name = QStringLiteral("%1-%2-%3-%4").arg(width).arg(modern ? "new" : "classic").arg(condition).arg(inspector ? "inspector" : "rail").toUtf8();
                        QTest::newRow(name.constData()) << width << modern << condition << inspector;
                    }
                }
            }
        }
    }
    void renderedFixtures() {
        NEREUS_SKIP_WITHOUT_QRHI();
        QFETCH(int, width); QFETCH(bool, modern); QFETCH(int, condition); QFETCH(bool, inspector);
        WorkspacePreview::setEnabledForTest(modern);
        PanadapterApplet pan(QStringLiteral("Synthetic RF fixture · no hardware"));
        pan.setWindowTitle(QStringLiteral("Synthetic IMD presentation fixture · no radio"));
        pan.resize(width, width == 320 ? 1120 : 760);
        SpectrumWidget* spectrum = pan.spectrumWidget();
        configure(*spectrum);
        spectrum->setImdInspectorVisible(inspector);
        spectrum->setImdGuidesVisible(true);
        pan.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pan));
        QTest::qWait(40);
        QVector<float> input = pixels(spectrum->width());
        if (condition == 1) { input[qRound(.875 * (input.size() - 1))] = -120; }
        if (condition == 2) { // Actual resolved fundamentals ground expected fifth products outside the visible grid.
            input.fill(-120); input[qRound(.2 * (input.size()-1))] = -22; input[qRound(.8 * (input.size()-1))] = -24;
        }
        ImdSpectrumTestAccess::seedVisiblePixels(*spectrum, input);
        if (condition == 3) { spectrum->setTestingIMD(false); }
        QCOMPARE(pan.width(), width);
        QVERIFY(pan.imdRail()->geometry().bottom() < spectrum->geometry().top());
        if (inspector) {
            QVERIFY(!pan.imdInspector()->geometry().intersects(spectrum->geometry()));
            for (QLabel* label : pan.imdInspector()->findChildren<QLabel*>()) {
                QVERIFY2(label->fontMetrics().height() >= 11, qPrintable(label->text()));
            }
        }
        const QImage cpu = ImdSpectrumTestAccess::render(*spectrum);
        if (condition == 0) {
            for (int index : {0, 2, 4}) { QVERIFY(inkPixels(cpu, PureSignalViewStyle::product(index)) > 8); }
        }
        if (condition == 3) {
            for (int index : {0, 2, 4}) { QCOMPARE(inkPixels(cpu, PureSignalViewStyle::product(index)), 0); }
        }
        const QByteArray directory = qgetenv("NEREUS_IMD_EVIDENCE");
        if (!directory.isEmpty()) {
            const QString path = QString::fromUtf8(directory);
            QDir().mkpath(path);
            const QString stem = QString::fromLatin1(QTest::currentDataTag());
            // Ordinary widgets are captured directly. Native spectrum evidence is independently checked below.
            QVERIFY(pan.grab().save(path + QStringLiteral("/task3-%1.png").arg(stem)));
            QVERIFY(ImdSpectrumTestAccess::render(*spectrum).save(path + QStringLiteral("/task3-cpu-%1.png").arg(stem)));
            qInfo() << "Synthetic fixture" << stem << "logical" << pan.size() << "DPR" << pan.devicePixelRatioF();
        }
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy frames(spectrum, &QRhiWidget::frameSubmitted);
        NEREUS_VERIFY_FRAME(NativeWindowFrames::requestFrame(*spectrum, frames, 2000));
        qInfo() << "Native QRhi frameSubmitted observed; framebuffer acceptance";
        const QImage gpu = spectrum->grabFramebuffer();
        QVERIFY(!gpu.isNull());
        if (condition == 0) { for (int index : {0, 2, 4}) { QVERIFY(inkPixels(gpu, PureSignalViewStyle::product(index)) > 8); } }
        if (!directory.isEmpty()) {
            const QString base = QString::fromUtf8(directory) + QStringLiteral("/task3-%1").arg(QString::fromLatin1(QTest::currentDataTag()));
            QVERIFY(gpu.save(QString::fromUtf8(directory) + QStringLiteral("/task3-native-%1.png").arg(QString::fromLatin1(QTest::currentDataTag()))));
            // Directly render ordinary panels at their actual DPR. They contain no QRhi children.
            const QImage rail = renderOrdinaryWidget(*pan.imdRail());
            const QImage detail = inspector ? renderOrdinaryWidget(*pan.imdInspector()) : QImage{};
            QVERIFY(rail.save(base + QStringLiteral("-rail-direct.png")));
            if (inspector) { QVERIFY(detail.save(base + QStringLiteral("-inspector-direct.png"))); }
            QImage composite(qRound(pan.width() * pan.devicePixelRatioF()), qRound(pan.height() * pan.devicePixelRatioF()), QImage::Format_ARGB32_Premultiplied);
            composite.setDevicePixelRatio(pan.devicePixelRatioF());
            composite.fill(pan.palette().window().color());
            QPainter painter(&composite);
            painter.drawImage(QRectF(pan.imdRail()->geometry()), rail);
            if (inspector) { painter.drawImage(QRectF(pan.imdInspector()->geometry()), detail); }
            painter.drawImage(QRectF(spectrum->geometry()), gpu);
            painter.drawImage(QRectF(pan.statusOverlay()->geometry()), renderOrdinaryWidget(*pan.statusOverlay()));
            painter.end();
            QVERIFY(composite.save(base + QStringLiteral("-native-composite.png")));
        }
#endif
        if (condition == 0) {
            const QImage withGuides = ImdSpectrumTestAccess::render(*spectrum);
            spectrum->setImdGuidesVisible(false);
            const QImage withoutGuides = ImdSpectrumTestAccess::render(*spectrum);
            QVERIFY(withGuides != withoutGuides);
#ifdef NEREUS_GPU_SPECTRUM
            NEREUS_VERIFY_FRAME(NativeWindowFrames::requestFrame(*spectrum, frames, 2000));
            const QImage noGuidesGpu = spectrum->grabFramebuffer();
            QVERIFY(gpu != noGuidesGpu);
            if (!directory.isEmpty()) { QVERIFY(noGuidesGpu.save(QString::fromUtf8(directory) + QStringLiteral("/task3-native-no-guides-%1.png").arg(QString::fromLatin1(QTest::currentDataTag())))); }
#endif
        }
        WorkspacePreview::setEnabledForTest(false);
    }
};
QTEST_MAIN(TestImdMeasurementPanel)
#include "tst_imd_measurement_panel.moc"
