// no-port-check: NereusSDR-original tests for the building-block reading list.
// Modification history (NereusSDR):
//   2026-10-26 - The Reading row offers the six TX stage peak readings
//                 (MeterBinding 113-118) under plain labels, and choosing
//                 one saves its id. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.

#include <QtTest>
#include <QComboBox>

#include "gui/containers/meter_property_editors/BaseItemEditor.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"

using namespace NereusSDR;

namespace {

class PlainItem final : public MeterItem {
public:
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter&, int, int) override {}
};

struct Expected { int id; const char* label; };

const Expected kPeaks[] = {
    {MeterBinding::TxMicPeak,     "TX: Mic Peak"},
    {MeterBinding::TxEqPeak,      "TX: EQ Peak"},
    {MeterBinding::TxLevelerPeak, "TX: Leveler Peak"},
    {MeterBinding::TxAlcPeak,     "TX: ALC Peak"},
    {MeterBinding::TxCompPeak,    "TX: Compressor Peak"},
    {MeterBinding::TxCfcPeak,     "TX: CFC Peak"},
};

// The Reading row's combo: the one whose list holds "(none)" and "RX: Signal Peak".
QComboBox* readingCombo(BaseItemEditor& editor)
{
    const auto combos = editor.findChildren<QComboBox*>();
    for (QComboBox* combo : combos) {
        if (combo->findText(QStringLiteral("RX: Signal Peak")) >= 0
            && combo->findText(QStringLiteral("(none)")) >= 0) {
            return combo;
        }
    }
    return nullptr;
}

} // namespace

class TestMeterReadingList : public QObject {
    Q_OBJECT
private slots:
    void offersTxPeaksWithPlainLabels()
    {
        PlainItem item;
        BaseItemEditor editor;
        editor.setItem(&item);
        QComboBox* combo = readingCombo(editor);
        QVERIFY(combo != nullptr);
        for (const Expected& e : kPeaks) {
            const int idx = combo->findData(e.id);
            QVERIFY2(idx >= 0, qPrintable(QStringLiteral("binding %1 missing").arg(e.id)));
            QCOMPARE(combo->itemText(idx), QString::fromLatin1(e.label));
        }
        for (int i = 0; i < combo->count(); ++i) {
            const QString text = combo->itemText(i);
            QVERIFY2(!text.contains(QStringLiteral("RX1")) && !text.contains(QStringLiteral("RX2"))
                         && !text.contains(QStringLiteral("VFO")),
                     qPrintable(text));
        }
    }

    void choosingPeakSavesItsId()
    {
        for (const Expected& e : kPeaks) {
            PlainItem item;
            item.setBindingId(MeterBinding::SignalPeak);
            BaseItemEditor editor;
            editor.setItem(&item);
            QComboBox* combo = readingCombo(editor);
            QVERIFY(combo != nullptr);
            QSignalSpy changed(&editor, &BaseItemEditor::propertyChanged);
            combo->setCurrentIndex(combo->findData(e.id));
            QCOMPARE(item.bindingId(), e.id);
            QVERIFY(changed.count() >= 1);

            // Reloading the item selects the saved reading again.
            BaseItemEditor reopened;
            reopened.setItem(&item);
            QComboBox* again = readingCombo(reopened);
            QVERIFY(again != nullptr);
            QCOMPARE(again->currentData().toInt(), e.id);
            QCOMPARE(again->currentText(), QString::fromLatin1(e.label));
        }
    }
};

QTEST_MAIN(TestMeterReadingList)
#include "tst_meter_reading_list.moc"
