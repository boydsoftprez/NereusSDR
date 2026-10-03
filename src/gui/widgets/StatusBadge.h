// src/gui/widgets/StatusBadge.h
#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QHBoxLayout;

namespace NereusSDR {

// StatusBadge — small badge with icon prefix + label, color-coded by variant.
// Used in the redesigned status bar (RX dashboard + right-side strip) and
// emits clicked / rightClicked so the host wires actions per design spec
// docs/architecture/2026-04-30-shell-chrome-redesign-design.md §Affordances.
class StatusBadge : public QWidget {
    Q_OBJECT

public:
    enum class Variant {
        Info,    // blue (#5fa8ff)   — type/mode badges (USB, AGC-S)
        On,      // green (#5fff8a)  — active toggle (NR2, NB, ANF, SQL)
        Off,     // dim grey (#404858) — inactive toggle (rendered only when
                 //                       host opts to show the off state)
        Warn,    // yellow (#ffd700) — degraded (jitter, high CPU)
        Tx,      // solid red (#ff6060) — TX MOX badge
    };

    explicit StatusBadge(QWidget* parent = nullptr);

    // The icon character (e.g. "~", "⨎", "⌁"). Single-character glyphs only.
    // Prefer setSvgIcon() for new code — fonts vary across platforms and
    // many monospace fallbacks lack the geometric/dingbat symbols we want
    // for status iconography. Setting an SVG icon clears any glyph icon
    // and vice versa.
    void setIcon(const QString& icon);
    // SVG-resource icon path (e.g. ":/icons/badge-check.svg"). The SVG is
    // rendered at 14×14 logical px and tinted with the variant's
    // foreground color via QPainter::CompositionMode_SourceIn — the SVG's
    // own colors are discarded, so authoring SVGs with any non-zero alpha
    // works (we recommend solid white for legibility while editing). The
    // icon re-tints automatically on setVariant().
    void setSvgIcon(const QString& resourcePath);
    // Label text (e.g. "USB", "2.4k", "NR2").
    void setLabel(const QString& label);
    void setVariant(Variant v);
    /// R-R3-21: a badge whose click opens something shows the hand cursor;
    /// the rest keep the arrow. Off by default.
    void setClickable(bool clickable);
    bool isClickable() const noexcept { return m_clickable; }

    QString icon() const noexcept { return m_icon; }
    QString svgIcon() const noexcept { return m_svgIcon; }
    QString label() const noexcept { return m_label; }
    Variant variant() const noexcept { return m_variant; }

signals:
    void clicked();
    void rightClicked(const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void applyStyle();
    void renderSvgIcon();
    void recomputeMinimumWidth();
    QColor variantForegroundColor() const;

    QString  m_icon;
    QString  m_svgIcon;
    QString  m_label;
    Variant  m_variant{Variant::Info};
    bool     m_clickable{false};
    QLabel*  m_iconLabel{nullptr};
    QLabel*  m_textLabel{nullptr};
};

} // namespace NereusSDR
