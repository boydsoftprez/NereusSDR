// =================================================================
// src/gui/setup/HardwareDdcRoutingPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for scope notes.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
//   2026-05-27 Phase 3F closeout (Sub-Epic E Tasks 8-10): replaced stub
//              with per-DDC override QTableWidget backed by AppSettings.
// =================================================================

#include "gui/setup/HardwareDdcRoutingPage.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

// Phase 3F closeout — DDC count for the override table.  Sized to the
// P2 Orion-class maximum so every supported board's full DDC bank is
// addressable; rows beyond the active codec's actual DDC count simply
// stay at "(auto)" with no effect.  Real per-board row-hiding lands when
// the codec layer publishes its DDC count to this page.
constexpr int kMaxDdcRows = 7;

// AppSettings key suffix shapes.  Per-MAC scoped via AppSettings'
// hardware/<mac>/<suffix> store (see setHardwareValue / hardwareValue).
QString ddcSliceKey(int ddc)
{
    return QStringLiteral("DdcOverride%1/Slice").arg(ddc);
}
QString ddcAdcKey(int ddc)
{
    return QStringLiteral("DdcOverride%1/Adc").arg(ddc);
}

} // namespace

HardwareDdcRoutingPage::HardwareDdcRoutingPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("DDC Routing"), model, parent)
{
    auto* group = addSection(QStringLiteral("DDC Routing (power-user override)"));
    auto* groupLayout = qobject_cast<QVBoxLayout*>(group->layout());

    auto* intro = new QLabel(
        QStringLiteral("By default, the active codec auto-assigns DDCs to slices "
                       "based on band + sample rate (see Phase 3F Sub-Epic B). "
                       "Override here if you need a specific DDC pinned to a "
                       "specific slice or ADC. Both columns default to (auto); "
                       "explicit picks persist per-MAC.\n\n"
                       "Note: codec-side consumption of these overrides is wired "
                       "as a follow-up; today the picks round-trip to settings "
                       "but do not yet steer the codec assignment."),
        group);
    intro->setWordWrap(true);
    if (groupLayout) {
        groupLayout->addWidget(intro);
    } else {
        group->layout()->addWidget(intro);
    }

    auto* table = new QTableWidget(group);
    table->setColumnCount(3);
    table->setRowCount(kMaxDdcRows);
    table->setHorizontalHeaderLabels({QStringLiteral("DDC"),
                                       QStringLiteral("Slice"),
                                       QStringLiteral("ADC")});
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Common combo populations.  Slice combo gets (auto) + Slice A..E so a
    // user can pin a DDC to any of the five maximum slices the SKU allows
    // (real cap enforcement still lives on RadioModel::maxSlices); ADC
    // combo gets (auto) + ADC0 + ADC1 so 1-ADC SKUs can simply leave their
    // pick at (auto) with no effect.
    const QStringList sliceItems = {QStringLiteral("(auto)"),
                                     QStringLiteral("Slice A"),
                                     QStringLiteral("Slice B"),
                                     QStringLiteral("Slice C"),
                                     QStringLiteral("Slice D"),
                                     QStringLiteral("Slice E")};
    const QStringList adcItems = {QStringLiteral("(auto)"),
                                   QStringLiteral("ADC0"),
                                   QStringLiteral("ADC1")};

    const QString mac = (this->model() != nullptr)
                            ? this->model()->currentRadioMac()
                            : QString{};

    auto& s = AppSettings::instance();

    for (int ddc = 0; ddc < kMaxDdcRows; ++ddc) {
        auto* ddcItem = new QTableWidgetItem(QStringLiteral("DDC%1").arg(ddc));
        ddcItem->setFlags(ddcItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(ddc, 0, ddcItem);

        auto* sliceCombo = new QComboBox(table);
        sliceCombo->addItems(sliceItems);
        table->setCellWidget(ddc, 1, sliceCombo);

        auto* adcCombo = new QComboBox(table);
        adcCombo->addItems(adcItems);
        table->setCellWidget(ddc, 2, adcCombo);

        // Restore persisted choices.  If no MAC is available (disconnected
        // at page-open time), defaults stay at (auto) and no persistence
        // round-trip fires; the page reloads the next time the user opens
        // it after a connect.
        if (!mac.isEmpty()) {
            const int sliceIdx = s.hardwareValue(mac, ddcSliceKey(ddc), 0).toInt();
            const int adcIdx   = s.hardwareValue(mac, ddcAdcKey(ddc), 0).toInt();
            if (sliceIdx >= 0 && sliceIdx < sliceItems.size()) {
                sliceCombo->setCurrentIndex(sliceIdx);
            }
            if (adcIdx >= 0 && adcIdx < adcItems.size()) {
                adcCombo->setCurrentIndex(adcIdx);
            }
        }

        // Persist on change.  Re-resolve the MAC at write time so that a
        // connect-after-open path picks up the now-known MAC; falling back
        // to no-op when still disconnected.
        connect(sliceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, ddc](int idx) {
            const QString liveMac = (this->model() != nullptr)
                                        ? this->model()->currentRadioMac()
                                        : QString{};
            if (liveMac.isEmpty()) { return; }
            AppSettings::instance().setHardwareValue(liveMac, ddcSliceKey(ddc), idx);
        });
        connect(adcCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, ddc](int idx) {
            const QString liveMac = (this->model() != nullptr)
                                        ? this->model()->currentRadioMac()
                                        : QString{};
            if (liveMac.isEmpty()) { return; }
            AppSettings::instance().setHardwareValue(liveMac, ddcAdcKey(ddc), idx);
        });
    }

    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setMinimumHeight(kMaxDdcRows * 28 + 32);
    if (groupLayout) {
        groupLayout->addWidget(table);
    } else {
        group->layout()->addWidget(table);
    }

    auto* resetBtn = new QPushButton(QStringLiteral("Reset all to automatic"), group);
    resetBtn->setToolTip(
        QStringLiteral("Clear every per-DDC override on this MAC. Each row "
                        "falls back to the codec's automatic assignment."));
    connect(resetBtn, &QPushButton::clicked, this, [table]() {
        for (int ddc = 0; ddc < kMaxDdcRows; ++ddc) {
            if (auto* sliceCombo =
                    qobject_cast<QComboBox*>(table->cellWidget(ddc, 1))) {
                sliceCombo->setCurrentIndex(0);  // (auto)
            }
            if (auto* adcCombo =
                    qobject_cast<QComboBox*>(table->cellWidget(ddc, 2))) {
                adcCombo->setCurrentIndex(0);  // (auto)
            }
        }
    });
    if (groupLayout) {
        groupLayout->addWidget(resetBtn);
    } else {
        group->layout()->addWidget(resetBtn);
    }

    contentLayout()->addStretch(1);
}

} // namespace NereusSDR
