#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/bandstack/BandStackPopup.h  (NereusSDR)
// =================================================================
//
// The popup a band button's right-click and the VFO display's long press open:
// one band's rows for one slice, at the pointer, drawn as desktop.html's .bpop.
// The rows and the row menu are the panel's own BandStackRowList. A click
// outside closes it, a row menu opened from it does not, and with no usable
// stack it says why instead of showing rows.
// Design: docs/architecture/2026-11-05-band-stack-design.md, sections 3 and 7.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QPoint>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <optional>

#include "gui/NereusGuiExport.h"
#include "models/Band.h"

class QLabel;
class QScrollArea;

namespace NereusSDR {

class BandStackAccess;
class BandStackRowList;
class SliceModel;

class NEREUS_GUI_EXPORT BandStackPopup : public QWidget {
    Q_OBJECT

public:
    // desktop.html's .bpop.
    static constexpr int kWidth = 190;
    static constexpr int kMaxHeight = 330;
    static constexpr int kPointerGap = 2;
    static constexpr int kScreenGap = 6;

    explicit BandStackPopup(BandStackAccess* access, QWidget* parent = nullptr);

    void setAccess(BandStackAccess* access);
    /// A non-empty reason shows in place of the rows.
    void setUnavailableReason(const QString& reason);

    /// Opens at the pointer, kept on the screen. Null `slice` says to select one.
    void showFor(SliceModel* slice, Band band, const QPoint& globalPos);

    BandStackRowList* rows() const { return m_rows; }
    std::optional<Band> band() const { return m_band; }
    /// What shows in place of the rows now, empty when the rows show.
    QString shownReason() const;

private:
    void refreshHeader();
    void refreshContent();
    void fitAndPlace();

    QPointer<BandStackAccess> m_access;
    QPointer<SliceModel> m_slice;
    QMetaObject::Connection m_sliceFrequency;
    QMetaObject::Connection m_document;
    std::optional<Band> m_band;
    QString m_reason;
    QPoint m_anchor;

    QLabel* m_chip{nullptr};
    QLabel* m_title{nullptr};
    QLabel* m_count{nullptr};
    QLabel* m_reasonLabel{nullptr};
    QScrollArea* m_scroll{nullptr};
    BandStackRowList* m_rows{nullptr};
};

} // namespace NereusSDR
