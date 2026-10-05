#pragma once
// no-port-check: NereusSDR-original editor for existing native action visibility.
// Modification history (NereusSDR):
//   2026-10-03 — Explicit individual control selection by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include "ButtonBoxItemEditor.h"
#include <QVector>

class QComboBox;
class QCheckBox;

namespace NereusSDR {
class OtherButtonItem;

class OtherButtonItemEditor : public ButtonBoxItemEditor {
    Q_OBJECT
public:
    explicit OtherButtonItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildOtherSpecific();
    void refreshSelection();
    QComboBox* m_singleControl = nullptr;
    QVector<QCheckBox*> m_visibility;

};

} // namespace NereusSDR
