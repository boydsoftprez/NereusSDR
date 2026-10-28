// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_passband_origin_drawing.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-24, R-CW-45): every passband drawer puts filter offset
// 0 at the passband origin, the dial plus the CW receive shift
// (SliceModel::passbandOriginHz()). CWU at 14.030 MHz with a 400..900 Hz
// filter at the 600 Hz pitch is drawn from 14.029800 to 14.030300 MHz on the
// pan and the waterfall; dragging its high edge to 14.030400 sets the
// filter high to 1000; the VFO line stays on the dial; USB is unchanged.
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, created (R-CW-24, R-CW-45). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int    kW = 800;
constexpr int    kH = 400;
constexpr double kDialHz = 14'030'000.0;
constexpr double kBandwidthHz = 8000.0;    // 10 Hz per pixel
constexpr int    kPitchHz = 600;
constexpr int    kCwLowHz = 400;
constexpr int    kCwHighHz = 900;
constexpr int    kSpecY = 50;

// Mirrors SpectrumWidget::hzToX with the dBm strip hidden, truncating cast
// included.
int xForHz(double hz)
{
    const double lowHz = kDialHz - kBandwidthHz / 2.0;
    return static_cast<int>((hz - lowHz) / kBandwidthHz * kW);
}

void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)), button, buttons,
                   Qt::NoModifier);
    QApplication::sendEvent(w, &me);
}

struct Pan {
    SpectrumWidget sw;
    Pan()
    {
        sw.resize(kW, kH);
        sw.setDbmScaleVisible(false);
        sw.setFrequencyRange(kDialHz, kBandwidthHz);
        sw.setConnectionState(ConnectionState::Connected);
        sw.setStepSize(10);
        sw.setCwPitchHz(kPitchHz);
    }
    VfoWidget* receiver(DSPMode mode, int low, int high)
    {
        VfoWidget* flag = sw.addVfoWidget(0);
        flag->setMode(mode);
        flag->setFrequency(kDialHz);
        flag->setFilter(low, high);
        flag->setActiveSlice(true);
        return flag;
    }
    // The own markers painted on black: the spectrum above, the waterfall
    // below.
    QImage paint()
    {
        QImage image(kW, kH, QImage::Format_ARGB32);
        image.fill(Qt::black);
        QPainter p(&image);
        sw.drawOwnMarkersForTest(p, QRect(0, 0, kW, kH / 2), QRect(0, kH / 2, kW, kH / 2));
        return image;
    }
};

bool tinted(const QImage& image, int x, int y)
{
    const QRgb px = image.pixel(x, y);
    return qRed(px) + qGreen(px) + qBlue(px) > 0;
}

} // namespace

class TestPassbandOriginDrawing : public QObject {
    Q_OBJECT
private slots:
    void theCwOffsetIsSliceModelsShift()
    {
        QCOMPARE(SpectrumWidget::cwPassbandOffsetHz(DSPMode::CWU, kPitchHz), -600.0);
        QCOMPARE(SpectrumWidget::cwPassbandOffsetHz(DSPMode::CWL, kPitchHz), 600.0);
        for (DSPMode mode : {DSPMode::USB, DSPMode::LSB, DSPMode::AM, DSPMode::DIGU}) {
            QCOMPARE(SpectrumWidget::cwPassbandOffsetHz(mode, kPitchHz), 0.0);
        }
        // And SliceModel's own origin at the default pitch agrees.
        AppSettings::instance().clear();
        SliceModel slice;
        slice.setDspMode(DSPMode::CWU);
        slice.setFrequency(kDialHz);
        QCOMPARE(slice.passbandOriginHz(),
                 kDialHz + SpectrumWidget::cwPassbandOffsetHz(DSPMode::CWU, kPitchHz));
    }

    void aCwuPassbandIsDrawnAroundTheDial()
    {
        Pan pan;
        pan.receiver(DSPMode::CWU, kCwLowHz, kCwHighHz);
        const auto geo = pan.sw.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QCOMPARE(geo[0].centreHz, kDialHz);   // the VFO line on the dial
        QCOMPARE(geo[0].originHz + geo[0].filterLowHz, 14'029'800.0);
        QCOMPARE(geo[0].originHz + geo[0].filterHighHz, 14'030'300.0);

        // Pixels: the shaded band covers 14.029800 to 14.030300 on the pan
        // and the waterfall, and not where the unshifted band would be.
        const QImage image = pan.paint();
        for (int y : {kSpecY, kH / 2 + 50}) {
            QVERIFY2(tinted(image, xForHz(14'029'850.0), y), "inside, below the dial");
            QVERIFY2(tinted(image, xForHz(14'030'250.0), y), "inside, above the dial");
            QVERIFY2(!tinted(image, xForHz(14'029'700.0), y), "below the band");
            QVERIFY2(!tinted(image, xForHz(14'030'600.0), y), "the unshifted band");
        }
    }

    void aCwlPassbandIsMirrored()
    {
        Pan pan;
        pan.receiver(DSPMode::CWL, -kCwHighHz, -kCwLowHz);
        const auto geo = pan.sw.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QCOMPARE(geo[0].originHz + geo[0].filterLowHz, 14'029'700.0);
        QCOMPARE(geo[0].originHz + geo[0].filterHighHz, 14'030'200.0);
    }

    void thePansOwnPairFollowsItsMode()
    {
        Pan pan;
        pan.sw.setVfoFrequency(kDialHz);
        pan.sw.setTxMode(DSPMode::CWU);
        pan.sw.setFilterOffset(kCwLowHz, kCwHighHz);
        QCOMPARE(pan.sw.ownPassbandOriginHz() + kCwLowHz, 14'029'800.0);
        QCOMPARE(pan.sw.ownPassbandOriginHz() + kCwHighHz, 14'030'300.0);
        // With no flag the own marker is drawn from it.
        const auto geo = pan.sw.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QCOMPARE(geo[0].centreHz, kDialHz);
        QCOMPARE(geo[0].originHz, kDialHz - kPitchHz);
        // A pitch change moves it, with no settings read per frame.
        pan.sw.setCwPitchHz(700);
        QCOMPARE(pan.sw.ownPassbandOriginHz(), kDialHz - 700.0);
        pan.sw.setTxMode(DSPMode::USB);
        QCOMPARE(pan.sw.ownPassbandOriginHz(), kDialHz);
    }

    void draggingTheHighEdgeSetsTheFilterHigh()
    {
        Pan pan;
        VfoWidget* flag = pan.receiver(DSPMode::CWU, kCwLowHz, kCwHighHz);
        QSignalSpy edged(flag, &VfoWidget::filterChanged);
        const int highX = xForHz(14'030'300.0);
        const int toX = xForHz(14'030'400.0);
        sendMouse(&pan.sw, QEvent::MouseButtonPress, QPoint(highX, kSpecY), Qt::LeftButton,
                  Qt::LeftButton);
        sendMouse(&pan.sw, QEvent::MouseMove, QPoint(toX, kSpecY), Qt::NoButton, Qt::LeftButton);
        sendMouse(&pan.sw, QEvent::MouseButtonRelease, QPoint(toX, kSpecY), Qt::LeftButton,
                  Qt::NoButton);
        QCOMPARE(edged.count(), 1);
        QCOMPARE(edged.first().at(0).toInt(), kCwLowHz);
        QCOMPARE(edged.first().at(1).toInt(), 1000);
        QCOMPARE(flag->frequency(), kDialHz);   // the dial does not move
    }

    void usbIsUnchanged()
    {
        Pan pan;
        VfoWidget* flag = pan.receiver(DSPMode::USB, 100, 2900);
        const auto geo = pan.sw.sliceMarkerGeometry();
        QCOMPARE(geo.size(), 1);
        QCOMPARE(geo[0].originHz, kDialHz);
        QCOMPARE(geo[0].centreHz, kDialHz);

        QSignalSpy edged(flag, &VfoWidget::filterChanged);
        // The high edge at +2900 drags by +100 to 3000.
        const int highX = xForHz(kDialHz + 2900.0);
        sendMouse(&pan.sw, QEvent::MouseButtonPress, QPoint(highX, kSpecY), Qt::LeftButton,
                  Qt::LeftButton);
        sendMouse(&pan.sw, QEvent::MouseMove, QPoint(highX + 10, kSpecY), Qt::NoButton,
                  Qt::LeftButton);
        sendMouse(&pan.sw, QEvent::MouseButtonRelease, QPoint(highX + 10, kSpecY),
                  Qt::LeftButton, Qt::NoButton);
        QCOMPARE(edged.count(), 1);
        QCOMPARE(edged.first().at(1).toInt(), 3000);
    }

    void aForeignCwMarkerIsDrawnAroundItsDial()
    {
        Pan pan;
        SpectrumWidget::ForeignSliceMarker marker;
        marker.sliceId = 1;
        marker.centreHz = kDialHz;
        marker.filterLowHz = kCwLowHz;
        marker.filterHighHz = kCwHighHz;
        marker.passbandOffsetHz = SpectrumWidget::cwPassbandOffsetHz(DSPMode::CWU, kPitchHz);
        marker.color = VfoWidget::sliceColor(1);
        marker.letter = QStringLiteral("B");
        pan.sw.setForeignSliceMarkers({marker});
        QImage image(kW, kH, QImage::Format_ARGB32);
        image.fill(Qt::black);
        {
            QPainter p(&image);
            pan.sw.drawForeignMarkersForTest(p, QRect(0, 0, kW, kH / 2),
                                             QRect(0, kH / 2, kW, kH / 2));
        }
        // The dashed edges sit at 14.029800 and 14.030300: some pixel in each
        // edge column is drawn; none at the unshifted edges.
        const auto columnDrawn = [&image](int x) {
            for (int y = 0; y < kH / 2; ++y) {
                if (tinted(image, x, y)) { return true; }
            }
            return false;
        };
        QVERIFY(columnDrawn(xForHz(14'029'800.0)));
        QVERIFY(columnDrawn(xForHz(14'030'300.0)));
        QVERIFY(!columnDrawn(xForHz(14'030'900.0)));
    }
};

QTEST_MAIN(TestPassbandOriginDrawing)
#include "tst_passband_origin_drawing.moc"
