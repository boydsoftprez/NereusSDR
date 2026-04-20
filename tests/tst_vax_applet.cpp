// =================================================================
// tests/tst_vax_applet.cpp  (NereusSDR)
// =================================================================
//
// Smoke tests for VaxApplet — Phase 3O Sub-Phase 9 Task 9.2b.
//
// Coverage is intentionally shallow (widget-level only): construction,
// signal plumbing, and AppSettings round-trip. RT-safety + gain/mute
// logic belong to tst_audio_engine_vax_gain (Task 9.2a).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QLabel>
#include <QPushButton>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "gui/applets/VaxApplet.h"
#include "gui/widgets/MeterSlider.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TstVaxApplet : public QObject {
    Q_OBJECT

private:
    // Helper: construct a live RadioModel + AudioEngine + VaxApplet triple.
    // Caller owns the RadioModel (keeps the AudioEngine + applet alive).
    // Move-only to keep unique_ptr semantics clean under NRVO.
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        VaxApplet* applet{nullptr};  // parented to nothing; caller deletes

        Harness() = default;
        Harness(const Harness&) = delete;
        Harness& operator=(const Harness&) = delete;
        Harness(Harness&& other) noexcept
            : radio(std::move(other.radio)), applet(other.applet) {
            other.applet = nullptr;
        }
        Harness& operator=(Harness&& other) noexcept {
            if (this != &other) {
                delete applet;
                radio = std::move(other.radio);
                applet = other.applet;
                other.applet = nullptr;
            }
            return *this;
        }
        ~Harness() {
            delete applet;
        }
    };

    Harness makeHarness() {
        Harness h;
        h.radio = std::make_unique<RadioModel>();
        h.applet = new VaxApplet(h.radio.get(), h.radio->audioEngine(), nullptr);
        return h;
    }

    // Reach into the applet to find a MeterSlider for channel `ch` (1..4).
    // VaxApplet::m_rxMeter is private so walk the children — every applet
    // row owns exactly one MeterSlider; the first four are RX (channel
    // order), the fifth is TX.
    MeterSlider* rxMeter(VaxApplet* a, int ch) {
        auto meters = a->findChildren<MeterSlider*>();
        Q_ASSERT(meters.size() >= VaxApplet::kChannels);
        return meters.at(ch - 1);
    }

    // Walk children for the channel `ch` mute QPushButton.
    QPushButton* muteButton(VaxApplet* a, int ch) {
        auto btns = a->findChildren<QPushButton*>();
        Q_ASSERT(btns.size() >= VaxApplet::kChannels);
        return btns.at(ch - 1);
    }

private slots:

    void init() {
        // Test isolation — each test starts from a clean settings store.
        AppSettings::instance().clear();
    }

    // ── 1. Construct without crashing; widget is non-null + visible ────
    void constructsWithoutCrash() {
        Harness h = makeHarness();
        QVERIFY(h.applet != nullptr);
        QCOMPARE(h.applet->appletId(), QStringLiteral("vax"));
        QCOMPARE(h.applet->appletTitle(), QStringLiteral("VAX"));

        h.applet->show();
        QVERIFY(h.applet->isVisible());
    }

    // ── 2. RX gain slider change reaches AudioEngine::setVaxRxGain ─────
    void rxGainSliderWritesToEngine() {
        Harness h = makeHarness();
        MeterSlider* m1 = rxMeter(h.applet, 1);
        QVERIFY(m1);

        // Direct gainChanged emission — MeterSlider's mouse path is
        // header-inline and not under test here.
        emit m1->gainChanged(0.6f);

        QCOMPARE(h.radio->audioEngine()->vaxRxGain(1), 0.6f);
    }

    // ── 3. RX gain slider change persists to AppSettings ──────────────
    void rxGainSliderPersists() {
        Harness h = makeHarness();
        MeterSlider* m1 = rxMeter(h.applet, 1);
        QVERIFY(m1);

        emit m1->gainChanged(0.75f);

        const QString got =
            AppSettings::instance().value("audio/Vax1/RxGain").toString();
        QCOMPARE(got.toFloat(), 0.75f);
    }

    // ── 4. Mute button toggle reaches engine + persists ───────────────
    void muteButtonToggles() {
        Harness h = makeHarness();
        QPushButton* b1 = muteButton(h.applet, 1);
        QVERIFY(b1);

        b1->click();  // was unchecked → now checked

        QCOMPARE(h.radio->audioEngine()->vaxMuted(1), true);
        QCOMPARE(AppSettings::instance().value("audio/Vax1/Muted").toString(),
                 QStringLiteral("True"));

        b1->click();  // back to unchecked
        QCOMPARE(h.radio->audioEngine()->vaxMuted(1), false);
        QCOMPARE(AppSettings::instance().value("audio/Vax1/Muted").toString(),
                 QStringLiteral("False"));
    }

    // ── 5. Saved gain applied to engine on construction ───────────────
    void savedGainAppliedOnConstruction() {
        AppSettings::instance().setValue("audio/Vax1/RxGain",
                                         QStringLiteral("0.250"));
        AppSettings::instance().setValue("audio/TxGain",
                                         QStringLiteral("0.333"));

        Harness h = makeHarness();
        Q_UNUSED(h);
        QCOMPARE(h.radio->audioEngine()->vaxRxGain(1), 0.25f);
        QCOMPARE(h.radio->audioEngine()->vaxTxGain(), 0.333f);
    }

    // ── 6. Slice assigned to a VAX channel shows up in the tags label ──
    void tagsLabelShowsSliceLetter() {
        Harness h = makeHarness();

        const int idx = h.radio->addSlice();
        SliceModel* s = h.radio->sliceAt(idx);
        QVERIFY(s);
        s->setSliceIndex(0);        // letter 'A'
        s->setVaxChannel(2);        // route to VAX 2

        // Find the labels in channel order — the applet's tags labels are
        // the only QLabels with the dim '\u2014' placeholder style; pick
        // the channel-2 label by walking children.
        auto labels = h.applet->findChildren<QLabel*>();
        // The layout appends in strict row order; tags label is the 2nd
        // label of each RX row, but it's easiest to just search by text.
        bool found = false;
        for (QLabel* l : labels) {
            if (l->text().contains(QStringLiteral("Slice A"))) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "No label contained 'Slice A' after assignment to VAX 2");
    }

    // ── 7. Removing a slice refreshes the tag row back to placeholders ──
    void sliceRemovedClearsTagLabel() {
        Harness h = makeHarness();

        const int idx = h.radio->addSlice();
        SliceModel* s = h.radio->sliceAt(idx);
        QVERIFY(s);
        s->setSliceIndex(0);
        s->setVaxChannel(3);

        auto hasSliceA = [&]() {
            for (QLabel* l : h.applet->findChildren<QLabel*>()) {
                if (l->text().contains(QStringLiteral("Slice A"))) {
                    return true;
                }
            }
            return false;
        };
        QVERIFY(hasSliceA());

        h.radio->removeSlice(idx);
        QVERIFY2(!hasSliceA(),
                 "Tag label still shows 'Slice A' after the slice was removed");
    }
};

QTEST_MAIN(TstVaxApplet)
#include "tst_vax_applet.moc"
