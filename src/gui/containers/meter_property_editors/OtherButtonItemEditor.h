#pragma once
// no-port-check: NereusSDR-original editor for existing native action visibility.
// Modification history (NereusSDR):
//   2026-10-26 - Lists every button NereusSDR offers under the Thetis
//                 options-grid headings, and a Reset layout button.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-03 — Explicit individual control selection by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include "ButtonBoxItemEditor.h"
#include "gui/meters/OtherButtonItem.h"

#include <QString>
#include <QVector>

#include <functional>

class QComboBox;
class QCheckBox;
class QPushButton;

namespace NereusSDR {

class OtherButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    // One "Visible controls" row: the button, the heading it sits under,
    // and the words shown for it (the Thetis caption and tooltip in
    // NereusSDR's wording: slices, not VFO A / B or RX1 / RX2).
    struct VisibilityRow {
        OtherButtonItem::ButtonId id;
        QString section;
        QString caption;
        QString tooltip;
    };
    // Every id `offered` accepts, in Thetis options-grid order.
    static QVector<VisibilityRow> visibilityRows(
        const std::function<bool(OtherButtonItem::ButtonId)>& offered);

    explicit OtherButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildOtherSpecific();
    void refreshSelection();
    QComboBox* m_singleControl = nullptr;
    QVector<QPair<OtherButtonItem::ButtonId, QCheckBox*>> m_visibility;
    QPushButton* m_resetLayout = nullptr;

};

} // namespace NereusSDR
