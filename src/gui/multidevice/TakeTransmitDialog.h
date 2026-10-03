#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/TakeTransmitDialog.h  (NereusSDR)
// =================================================================
//
// "Take transmit from <short name>?" (iPhone app plan Task 78, R-IOS-02,
// R-IOS-30; the several-devices design, section 12 item 6, ruling 8.7).
// Asked in the window first, from what the Core's txState shows, and
// again, in the same shape, when the Core asks (confirm.request
// `takeTransmit`, its `holder`). The button is the red "Unkey and take
// over" while the holder is on the air.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-02, R-IOS-30),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: TX badge take: shortNameOf, the name the question uses.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLabel;
class QPushButton;

namespace NereusSDR {

class TransmitState;

class TakeTransmitDialog : public QDialog {
    Q_OBJECT

public:
    /// Who holds transmit, as the question shows it.
    struct Holder {
        QString name;
        QString shortName;
        bool onAir = false;
        bool away = false;
        qint64 awayForSeconds = 0;
    };

    explicit TakeTransmitDialog(const Holder& holder, QWidget* parent = nullptr);

    /// From the Core's txState (the window asks first).
    static Holder fromTransmitState(const TransmitState& tx);
    /// From a takeTransmit question's `holder`.
    static Holder fromHolderEntry(const QJsonObject& holder);

    static QString questionText(const Holder& holder);
    /// The holder as the question names it (short name, else name).
    static QString shortNameOf(const Holder& holder);
    static QString detailText(const Holder& holder);
    static QString takeButtonText(bool onAir);

    QLabel* questionLabel() const { return m_question; }
    QLabel* detailLabel() const { return m_detail; }
    QPushButton* takeButton() const { return m_take; }
    QPushButton* cancelButton() const { return m_cancel; }
    bool redButton() const { return m_red; }

private:
    QLabel* m_question = nullptr;
    QLabel* m_detail = nullptr;
    QPushButton* m_take = nullptr;
    QPushButton* m_cancel = nullptr;
    bool m_red = false;
};

} // namespace NereusSDR
