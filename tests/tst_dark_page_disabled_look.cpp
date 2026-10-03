// =================================================================
// tests/tst_dark_page_disabled_look.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No upstream port.
//
// no-port-check: test fixture, no Thetis attribution required.
//
// A control that cannot run is shown disabled with its reason, so on a
// page styled by Style::applyDarkPageStyle a disabled control must not
// look like an enabled one, and an enabled one must keep the page style.
// The disabled colours are the style guide's disabled trio
// (Style::kDisabledText / kDisabledBg / kDisabledBorder).
//
// Modification history (NereusSDR):
//   2026-09-26 - Written by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"
#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

QColor hex(const char* value)
{
    return QColor(QString::fromLatin1(value));
}

// One enabled and one disabled copy of a control on a dark-styled page.
struct Pair {
    QWidget* enabled = nullptr;
    QWidget* disabled = nullptr;
};

template <typename W, typename Make>
Pair addPair(QWidget& page, Make make)
{
    Pair pair;
    W* on = make();
    W* off = make();
    on->setParent(&page);
    off->setParent(&page);
    page.layout()->addWidget(on);
    page.layout()->addWidget(off);
    off->setEnabled(false);
    pair.enabled = on;
    pair.disabled = off;
    return pair;
}

QColor foreground(QWidget* w, QPalette::ColorGroup group)
{
    return w->palette().color(group, w->foregroundRole());
}

} // namespace

class TestDarkPageDisabledLook : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::instance().clear();
    }

    // Text-bearing controls: the disabled palette carries the style
    // guide's disabled text colour; the enabled one keeps the primary text.
    void aDisabledControlsPaletteDiffersFromAnEnabledOnes()
    {
        QWidget page;
        page.setLayout(new QVBoxLayout);
        Style::applyDarkPageStyle(&page);

        QList<QPair<QString, Pair>> pairs;
        pairs.append({QStringLiteral("QLabel"),
                      addPair<QLabel>(page, [] { return new QLabel(QStringLiteral("Label")); })});
        pairs.append({QStringLiteral("QCheckBox"), addPair<QCheckBox>(page, [] {
                          return new QCheckBox(QStringLiteral("Check"));
                      })});
        pairs.append({QStringLiteral("QRadioButton"), addPair<QRadioButton>(page, [] {
                          return new QRadioButton(QStringLiteral("Radio"));
                      })});
        pairs.append({QStringLiteral("QSpinBox"),
                      addPair<QSpinBox>(page, [] { return new QSpinBox; })});
        pairs.append({QStringLiteral("QDoubleSpinBox"),
                      addPair<QDoubleSpinBox>(page, [] { return new QDoubleSpinBox; })});
        pairs.append({QStringLiteral("QComboBox"), addPair<QComboBox>(page, [] {
                          auto* combo = new QComboBox;
                          combo->addItem(QStringLiteral("Item"));
                          return combo;
                      })});
        pairs.append({QStringLiteral("QLineEdit"),
                      addPair<QLineEdit>(page, [] { return new QLineEdit(QStringLiteral("Text")); })});
        pairs.append({QStringLiteral("QPushButton"), addPair<QPushButton>(page, [] {
                          return new QPushButton(QStringLiteral("Button"));
                      })});
        pairs.append({QStringLiteral("QGroupBox"), addPair<QGroupBox>(page, [] {
                          return new QGroupBox(QStringLiteral("Group"));
                      })});

        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        for (const auto& [name, pair] : pairs) {
            pair.enabled->ensurePolished();
            pair.disabled->ensurePolished();
            const QColor on = foreground(pair.enabled, QPalette::Active);
            const QColor off = foreground(pair.disabled, QPalette::Disabled);
            QVERIFY2(on != off, qPrintable(name + QStringLiteral(": disabled looks enabled")));
            QVERIFY2(off == hex(Style::kDisabledText),
                     qPrintable(name + QStringLiteral(": disabled text is ") + off.name()));
            QVERIFY2(on == hex(Style::kTextPrimary) || name == QStringLiteral("QGroupBox"),
                     qPrintable(name + QStringLiteral(": enabled text changed to ") + on.name()));
        }
    }

    // Controls whose state shows in drawn sub-controls (a slider's groove
    // and handle, a check box's indicator): a disabled one draws differently.
    void aDisabledSliderAndCheckedBoxDrawDifferently()
    {
        QWidget page;
        page.setLayout(new QVBoxLayout);
        Style::applyDarkPageStyle(&page);
        const Pair sliders = addPair<QSlider>(page, [] {
            auto* slider = new QSlider(Qt::Horizontal);
            slider->setRange(0, 100);
            slider->setValue(60);
            slider->setFixedSize(200, 20);
            return slider;
        });
        const Pair boxes = addPair<QCheckBox>(page, [] {
            auto* box = new QCheckBox;
            box->setChecked(true);
            box->setFixedSize(20, 20);
            return box;
        });
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        QCOMPARE(sliders.enabled->size(), sliders.disabled->size());
        QVERIFY(sliders.enabled->grab().toImage() != sliders.disabled->grab().toImage());
        QCOMPARE(boxes.enabled->size(), boxes.disabled->size());
        QVERIFY(boxes.enabled->grab().toImage() != boxes.disabled->grab().toImage());
    }

    // A real Setup page: Transmit > Power disables the fixed tune power box
    // until the fixed drive source is chosen; the SWR limit box is enabled.
    void aRealSetupPageShowsItsDisabledBoxDisabled()
    {
        RadioModel model;
        PowerPage page(&model);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));

        QAbstractSpinBox* disabledBox = nullptr;
        QAbstractSpinBox* enabledBox = nullptr;
        for (QAbstractSpinBox* box : page.findChildren<QAbstractSpinBox*>()) {
            if (!box->isEnabled() && !disabledBox) {
                disabledBox = box;
            }
            if (box->isEnabled() && !enabledBox) {
                enabledBox = box;
            }
        }
        QVERIFY2(disabledBox, "no disabled spin box on the Power page");
        QVERIFY2(enabledBox, "no enabled spin box on the Power page");
        QCOMPARE(foreground(disabledBox, QPalette::Disabled), hex(Style::kDisabledText));
        QVERIFY(foreground(enabledBox, QPalette::Active) != hex(Style::kDisabledText));
    }
};

QTEST_MAIN(TestDarkPageDisabledLook)
#include "tst_dark_page_disabled_look.moc"
