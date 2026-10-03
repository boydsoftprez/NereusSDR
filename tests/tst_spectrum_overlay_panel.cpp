// =================================================================
// tests/tst_spectrum_overlay_panel.cpp  (NereusSDR)
// =================================================================
//
// Smoke tests for SpectrumOverlayPanel::setRadioModel() — Phase 3O
// Sub-Phase 9 Task 9.2c (issue #70 fold-in).
//
// Coverage: the VAX Ch combo on the left-edge overlay is wired
// bidirectionally to slice 0's vaxChannel() with echo prevention.
// The IQ Ch combo stays disabled (feature-flagged per design spec
// §6.7/§11.3).
//
// 2026-09-25 (R-IOS-27, R-IOS-06): the BAND flyout draws the same twelve
// buttons, in the same places, and emits the same bandSelected arguments
// now that its table lives in models/BandGrid.h. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-09-30 (TX rulings, item 3): the ATT flyout is held with the reason
// on a pan whose slice this window only listens to, and writes nothing.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/StepAttenuatorFacade.h"
#include "gui/SpectrumOverlayPanel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TstSpectrumOverlayPanel : public QObject {
    Q_OBJECT

private:
    // SpectrumOverlayPanel parents its flyouts to `parentWidget()` (the
    // host SpectrumWidget in production — a bare QWidget in these tests).
    // Give it a host parent and search from there so findChild reaches the
    // combo's setObjectName("vaxCombo") in buildVaxFlyout.
    struct PanelHarness {
        QWidget host;
        SpectrumOverlayPanel* panel{nullptr};
        PanelHarness() {
            panel = new SpectrumOverlayPanel(&host);
        }
    };

    QComboBox* vaxCombo(PanelHarness& h) {
        return h.host.findChild<QComboBox*>(QStringLiteral("vaxCombo"));
    }

    QComboBox* rxAntennaCombo(PanelHarness& h) {
        return h.host.findChild<QComboBox*>(QStringLiteral("m_rxAntCmb"));
    }

    QComboBox* txAntennaCombo(PanelHarness& h) {
        return h.host.findChild<QComboBox*>(QStringLiteral("m_txAntCmb"));
    }

private slots:

    void init() {
        AppSettings::instance().clear();
    }

    // ── 1. setRadioModel enables the VAX combo once slice 0 exists ─────
    void setRadioModelEnablesVaxCombo() {
        RadioModel radio;
        radio.addSlice();  // slice 0

        PanelHarness h;
        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);
        QVERIFY(!combo->isEnabled());  // unbound → disabled
        // Fix wave M6: the unbound tooltip is in plain operator words, not
        // "not yet bound to a radio model".
        for (const QString& word : {QStringLiteral("model"), QStringLiteral("bound"),
                                    QStringLiteral("not yet")}) {
            QVERIFY2(!combo->toolTip().contains(word, Qt::CaseInsensitive),
                     qPrintable(combo->toolTip()));
        }
        QVERIFY(OperatorWording::isPlain(combo->toolTip()));

        h.panel->setRadioModel(&radio);

        QVERIFY(combo->isEnabled());
        QVERIFY(combo->toolTip().contains("VAX", Qt::CaseInsensitive));
    }

    // ── 2. User picking an index writes through to slice 0 ─────────────
    void vaxComboWritesToSlice() {
        RadioModel radio;
        radio.addSlice();
        SliceModel* s = radio.sliceById(0);
        QVERIFY(s);

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);

        combo->setCurrentIndex(2);  // "2" → vaxChannel 2
        QCOMPARE(s->vaxChannel(), 2);

        combo->setCurrentIndex(0);  // back to Off
        QCOMPARE(s->vaxChannel(), 0);
    }

    // ── 3. Setting vaxChannel on the model echoes into the combo ───────
    void sliceUpdateEchoesToCombo() {
        RadioModel radio;
        radio.addSlice();
        SliceModel* s = radio.sliceById(0);
        QVERIFY(s);

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);

        s->setVaxChannel(3);
        QCOMPARE(combo->currentIndex(), 3);

        s->setVaxChannel(0);
        QCOMPARE(combo->currentIndex(), 0);
    }

    // ── 4. No feedback loop on echo (exactly one emission per setVax) ──
    void noFeedbackLoopOnEcho() {
        RadioModel radio;
        radio.addSlice();
        SliceModel* s = radio.sliceById(0);
        QVERIFY(s);

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QSignalSpy spy(s, &SliceModel::vaxChannelChanged);

        s->setVaxChannel(2);

        // If the combo's currentIndexChanged re-entered into
        // setVaxChannel, we would see 2 emissions here (second one
        // would no-op at the guard in SliceModel::setVaxChannel, but
        // the sequence would still pass through the combo callback
        // twice). The m_updatingFromModel flag short-circuits the
        // widget→model path so exactly one emission reaches the spy.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
    }

    // ── 5. Removed controls are gone (R-R3-49) ─────────────────────────
    // The operator removed the RF Gain slider, the WNB button and the
    // IQ channel combo from the overlay: none is built at all.
    void removedControlsAreGone() {
        RadioModel radio;
        radio.addSlice();

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QVERIFY(h.host.findChild<QComboBox*>(QStringLiteral("vaxIqCombo")) == nullptr);
        for (QLabel* l : h.host.findChildren<QLabel*>()) {
            QVERIFY2(l->text() != QStringLiteral("RF Gain:"), "RF Gain slider label still built");
            QVERIFY2(l->text() != QStringLiteral("IQ Ch"), "IQ channel label still built");
        }
        for (QPushButton* b : h.host.findChildren<QPushButton*>()) {
            QVERIFY2(b->text() != QStringLiteral("WNB"), "WNB button still built");
            QVERIFY2(!b->toolTip().contains(QStringLiteral("RF gain"), Qt::CaseInsensitive),
                     "a button still offers RF gain");
        }
    }

    // ── 5b. No ANT button over an empty flyout (R-R3-49, R-R3-21) ──────
    // On a board with no antenna choices (Hermes Lite 2, Atlas) both ANT
    // rows hide, so the ANT button is not shown either, as an empty Setup
    // page is not offered. A board with antenna choices keeps it, and the
    // strip keeps its other buttons in both cases.
    void antButtonHiddenWhenItsFlyoutWouldBeEmpty() {
        const auto antButton = [](PanelHarness& h) -> QPushButton* {
            for (QPushButton* b : h.host.findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("ANT")) { return b; }
            }
            return nullptr;
        };
        const auto shownButtons = [](PanelHarness& h) {
            int n = 0;
            for (QPushButton* b : h.panel->findChildren<QPushButton*>(
                     QString(), Qt::FindDirectChildrenOnly)) {
                if (!b->isHidden()) { ++n; }
            }
            return n;
        };
        struct Case { HPSDRHW board; bool antennas; const char* name; };
        const Case cases[] = {
            {HPSDRHW::HermesLite, false, "Hermes Lite 2"},
            {HPSDRHW::Atlas, false, "Atlas"},
            {HPSDRHW::OrionMKII, true, "ANAN-7000DLE"},
            {HPSDRHW::Saturn, true, "ANAN-G2"},
        };
        for (const Case& c : cases) {
            PanelHarness h;
            QPushButton* ant = antButton(h);
            QVERIFY(ant != nullptr);
            QVERIFY2(!ant->isHidden(), c.name);  // before any radio's caps
            const int before = shownButtons(h);
            const int heightBefore = h.panel->height();

            h.panel->setBoardCapabilities(BoardCapsTable::forBoard(c.board));
            QCOMPARE(!ant->isHidden(), c.antennas);
            QCOMPARE(shownButtons(h), c.antennas ? before : before - 1);
            QCOMPARE(h.panel->height() < heightBefore, !c.antennas);
            if (c.antennas) {
                QVERIFY2(rxAntennaCombo(h)->count() > 0, c.name);
            } else {
                QCOMPARE(rxAntennaCombo(h)->count(), 0);
            }
        }

        // A board change back to one with antennas brings the button back.
        PanelHarness h;
        h.panel->setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::HermesLite));
        QVERIFY(antButton(h)->isHidden());
        h.panel->setBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::Saturn));
        QVERIFY(!antButton(h)->isHidden());
    }

    // ── 6. Removing slice 0 disables the combo; replacing it rebinds ───
    void sliceZeroReplacedRebindsCombo() {
        RadioModel radio;
        radio.addSlice();  // slice 0 (A)
        SliceModel* first = radio.sliceById(0);
        QVERIFY(first);
        first->setVaxChannel(2);

        // Phase 3F Sub-Epic C Task 7 added a "never remove the last
        // remaining slice" invariant to RadioModel::removeSlice, which this
        // test predates. Keep a second slice alive so removing slice 0
        // below is legal. It takes id 1, so sliceById(0) still returns
        // nothing once A is gone, and the re-add reclaims id 0.
        const int keeper = radio.addSlice();
        QCOMPARE(keeper, 1);

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);
        QCOMPARE(combo->currentIndex(), 2);
        QVERIFY(combo->isEnabled());

        // Remove slice 0. The combo should reset to Off and disable.
        // `first` is now dangling — do not dereference it below.
        radio.removeSlice(0);
        QCOMPARE(combo->currentIndex(), 0);
        QVERIFY(!combo->isEnabled());

        // Add a replacement slice; the combo must rebind to the new
        // slice (Model→Widget) and route writes back (Widget→Model).
        radio.addSlice();
        SliceModel* second = radio.sliceById(0);
        QVERIFY(second);
        QVERIFY(combo->isEnabled());

        second->setVaxChannel(4);
        QCOMPARE(combo->currentIndex(), 4);

        combo->setCurrentIndex(1);
        QCOMPARE(second->vaxChannel(), 1);
    }

    // ── 7. setRadioModel before any slice exists defers the bind ───────
    void setRadioModelDefersWhenNoSlice() {
        RadioModel radio;  // no slices yet

        PanelHarness h;
        h.panel->setRadioModel(&radio);

        QComboBox* combo = vaxCombo(h);
        QVERIFY(combo);
        QVERIFY(!combo->isEnabled());  // still disabled — waiting

        // Now add slice 0 — sliceAdded should trigger the deferred bind.
        radio.addSlice();

        QVERIFY(combo->isEnabled());

        // Sanity: forward path works after the deferred bind.
        SliceModel* s = radio.sliceById(0);
        QVERIFY(s);
        combo->setCurrentIndex(4);
        QCOMPARE(s->vaxChannel(), 4);
    }

    void controls_bind_to_the_resolved_pan_slice()
    {
        RadioModel radio;
        SliceModel* first = radio.sliceById(radio.addSlice());
        SliceModel* second = radio.sliceById(radio.addSlice());
        QVERIFY(first);
        QVERIFY(second);
        first->setRxAntenna(QStringLiteral("ANT1"));
        first->setTxAntenna(QStringLiteral("ANT1"));
        first->setVaxChannel(1);
        second->setRxAntenna(QStringLiteral("ANT2"));
        second->setTxAntenna(QStringLiteral("ANT3"));
        second->setVaxChannel(4);

        PanelHarness h;
        h.panel->setSliceResolver([second]() { return second; });
        h.panel->setRadioModel(&radio);

        QComboBox* rx = rxAntennaCombo(h);
        QComboBox* tx = txAntennaCombo(h);
        QComboBox* vax = vaxCombo(h);
        QVERIFY(rx);
        QVERIFY(tx);
        QVERIFY(vax);
        QCOMPARE(rx->currentText(), QStringLiteral("ANT2"));
        QCOMPARE(tx->currentText(), QStringLiteral("ANT3"));
        QCOMPARE(vax->currentIndex(), 4);

        rx->setCurrentText(QStringLiteral("ANT3"));
        tx->setCurrentText(QStringLiteral("ANT2"));
        vax->setCurrentIndex(2);
        QCOMPARE(second->rxAntenna(), QStringLiteral("ANT3"));
        QCOMPARE(second->txAntenna(), QStringLiteral("ANT2"));
        QCOMPARE(second->vaxChannel(), 2);
        QCOMPARE(first->rxAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(first->txAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(first->vaxChannel(), 1);
    }

    void changing_the_pan_slice_rebinds_and_missing_slice_disables()
    {
        RadioModel radio;
        SliceModel* first = radio.sliceById(radio.addSlice());
        SliceModel* second = radio.sliceById(radio.addSlice());
        QVERIFY(first);
        QVERIFY(second);
        first->setVaxChannel(1);
        second->setVaxChannel(3);

        QPointer<SliceModel> resolved = first;
        PanelHarness h;
        h.panel->setSliceResolver([&resolved]() { return resolved.data(); });
        h.panel->setRadioModel(&radio);
        QComboBox* vax = vaxCombo(h);
        QVERIFY(vax);
        QCOMPARE(vax->currentIndex(), 1);

        resolved = second;
        h.panel->bindToPanSlice();
        QCOMPARE(vax->currentIndex(), 3);

        radio.removeSlice(second->sliceIndex());
        resolved = nullptr;
        h.panel->bindToPanSlice();
        QVERIFY(!vax->isEnabled());

        resolved = first;
        h.panel->bindToPanSlice();
        QVERIFY(vax->isEnabled());
        QCOMPARE(vax->currentIndex(), 1);
    }

    // The BAND flyout as it was drawn before its table moved to
    // models/BandGrid.h: four to a row, each emitting the name, frequency
    // and mode it always did, and 2 m after 6 m (R-IOS-26), which puts WWV
    // on a fourth row: thirteen buttons.
    void bandFlyoutDrawsAsBefore() {
        PanelHarness h;
        QGridLayout* grid = nullptr;
        for (QGridLayout* candidate : h.host.findChildren<QGridLayout*>()) {
            if (candidate->count() == 13) {
                grid = candidate;
            }
        }
        QVERIFY(grid);
        const struct {
            const char* label;
            const char* name;
            double freqHz;
            const char* mode;
        } expected[] = {
            {"160", "160m", 1.8e6, "LSB"},   {"80", "80m", 3.5e6, "LSB"},
            {"60", "60m", 5.3e6, "USB"},     {"40", "40m", 7.0e6, "LSB"},
            {"30", "30m", 10.1e6, "DIGU"},   {"20", "20m", 14.0e6, "USB"},
            {"17", "17m", 18.068e6, "USB"},  {"15", "15m", 21.0e6, "USB"},
            {"12", "12m", 24.89e6, "USB"},   {"10", "10m", 28.0e6, "USB"},
            {"6", "6m", 50.0e6, "USB"},      {"2", "2m", 144.2e6, "USB"},
            {"WWV", "WWV", 10.0e6, "AM"},
        };
        QSignalSpy selected(h.panel, &SpectrumOverlayPanel::bandSelected);
        for (int i = 0; i < 13; ++i) {
            QLayoutItem* item = grid->itemAtPosition(i / 4, i % 4);
            QVERIFY(item);
            auto* button = qobject_cast<QPushButton*>(item->widget());
            QVERIFY(button);
            QCOMPARE(button->text(), QString::fromLatin1(expected[i].label));
            QCOMPARE(button->size(), QSize(48, 26));
            button->click();
            QCOMPARE(selected.count(), i + 1);
            QCOMPARE(selected.last().at(0).toString(), QString::fromLatin1(expected[i].name));
            QCOMPARE(selected.last().at(1).toDouble(), expected[i].freqHz);
            QCOMPARE(selected.last().at(2).toString(), QString::fromLatin1(expected[i].mode));
        }
    }
    // TX rulings (item 3, JJ): a listened pan's ATT flyout is held with
    // the reason naming the controller, and writes nothing.
    void attFlyoutHeldOnAListenedSlice() {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setBoardForTest(HPSDRHW::Hermes);
        remote.addSlice();
        StepAttenuatorFacade* att = remote.stepAttFacade();
        att->setWindowAvailability(true, QString());
        att->applyRemoteProperty("enabled", true);

        PanelHarness h;
        h.panel->setRadioModel(&remote);
        auto* chk = h.host.findChild<QCheckBox*>(QStringLiteral("attEnableCheck"));
        auto* spin = h.host.findChild<QSpinBox*>(QStringLiteral("attSpin"));
        auto* reason = h.host.findChild<QLabel*>(QStringLiteral("attReason"));
        QVERIFY(chk && spin && reason);
        QVERIFY(chk->isEnabled());
        QVERIFY(spin->isEnabled());

        const int dB = att->attenuationDb();
        const QString held = QStringLiteral("Shack iPad controls this slice");
        h.panel->setAttHeldReason(held);
        QVERIFY(!chk->isEnabled());
        QVERIFY(!spin->isEnabled());
        QCOMPARE(chk->toolTip(), held);
        QCOMPARE(spin->toolTip(), held);
        QCOMPARE(reason->text(), held);
        QVERIFY(!reason->isHidden());
        chk->setChecked(false);
        spin->setValue(dB == 20 ? 21 : 20);
        QVERIFY(att->enabled());
        QCOMPARE(att->attenuationDb(), dB);

        h.panel->setAttHeldReason(QString());
        QVERIFY(chk->isEnabled());
        QVERIFY(spin->isEnabled());
        QVERIFY(chk->toolTip() != held);
        QVERIFY(reason->isHidden());
    }
};

QTEST_MAIN(TstSpectrumOverlayPanel)
#include "tst_spectrum_overlay_panel.moc"
