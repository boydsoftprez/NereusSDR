// no-port-check: NereusSDR logical-object typography; upstream studied, no port.
// Modification history (NereusSDR):
//   2026-11-09 - fitObjectText remembers a fitted font. Meter paints were
//                 running the search again for every unchanged label, and
//                 that work sat on the GUI thread ahead of the waterfall.
//   2026-10-26 - drawObjectTextElided: text that does not fit its box at the
//                 11 px minimum ends in an ellipsis instead of being cut on
//                 both sides. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#pragma once
#include "gui/styles/WorkspacePreview.h"
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QRectF>
#include <QStringList>
#include <QVector>
#include <cmath>
namespace NereusSDR {
// Reference dimensions are stable native design calibrations, never a saved
// height, a physical texture size or the shared canvas dimensions.
inline double objectTextScale(const QRectF& object, const QSizeF& reference) {
    return qMax(0.0,qMin(object.width()/reference.width(),object.height()/reference.height()));
}

// Identity of one fitObjectText result. The search overwrites the requested
// size, so the input font's size is not part of the key. The stored font is
// the search's return value.
struct ObjectTextFitKey {
    QString families;
    QString styleName;
    QString extras;
    int weight = 0;
    int style = 0;
    int stretch = 0;
    int styleHint = 0;
    int styleStrategy = 0;
    int capitalization = 0;
    int letterSpacingType = 0;
    int hinting = 0;
    double letterSpacing = 0;
    double wordSpacing = 0;
    bool underline = false;
    bool strikeOut = false;
    bool overline = false;
    bool kerning = false;
    bool fixedPitch = false;
    bool points = false;
    bool floor = false;
    double requested = 0;
    double boxWidth = 0;
    double boxHeight = 0;
    double logicalDpi = 0;
    double devicePixelRatio = 1;
    int flags = 0;
    QString text;
    bool operator==(const ObjectTextFitKey&) const = default;
};

inline size_t qHash(const ObjectTextFitKey& key, size_t seed = 0) noexcept
{
    // qHashMulti, not qHash: this overload would hide the Qt overloads.
    return qHashMulti(seed, key.families, key.styleName, key.extras, key.weight, key.style,
                      key.stretch, key.styleHint, key.styleStrategy, key.capitalization,
                      key.letterSpacingType, key.hinting, key.letterSpacing, key.wordSpacing,
                      key.underline, key.strikeOut, key.overline, key.kerning, key.fixedPitch,
                      key.points, key.floor, key.requested, key.boxWidth, key.boxHeight,
                      key.logicalDpi, key.devicePixelRatio, key.flags, key.text);
}

inline ObjectTextFitKey objectTextFitKey(const QFont& font, double requested, const QString& text,
                                         const QRectF& box, bool points, int flags, const QPaintDevice* device)
{
    ObjectTextFitKey key;
    const QStringList families = font.families();
    key.families = families.isEmpty() ? font.family() : families.join(QLatin1Char('\n'));
    key.styleName = font.styleName();
    key.weight = font.weight();
    key.style = int(font.style());
    key.stretch = font.stretch();
    key.styleHint = int(font.styleHint());
    key.styleStrategy = int(font.styleStrategy());
    key.capitalization = int(font.capitalization());
    key.letterSpacingType = int(font.letterSpacingType());
    key.hinting = int(font.hintingPreference());
    key.letterSpacing = font.letterSpacing();
    key.wordSpacing = font.wordSpacing();
    key.underline = font.underline();
    key.strikeOut = font.strikeOut();
    key.overline = font.overline();
    key.kerning = font.kerning();
    key.fixedPitch = font.fixedPitch();
    key.points = points;
    key.floor = WorkspacePreview::enabled();
    key.requested = requested;
    key.boxWidth = box.width();
    key.boxHeight = box.height();
    key.logicalDpi = device ? device->logicalDpiY() : -1.0;
    key.devicePixelRatio = device ? device->devicePixelRatioF() : 1.0;
    key.flags = flags;
    key.text = text;
    const QList<QFont::Tag> features = font.featureTags();
    const QList<QFont::Tag> axes = font.variableAxisTags();
    if (!features.isEmpty() || !axes.isEmpty()) {
        QStringList parts;
        parts.reserve(features.size() + axes.size());
        for (const QFont::Tag& tag : features) {
            parts.append(QString::number(tag.value()) + QLatin1Char('=') + QString::number(font.featureValue(tag)));
        }
        for (const QFont::Tag& tag : axes) {
            parts.append(QString::number(tag.value()) + QLatin1Char(':') + QString::number(font.variableAxisValue(tag), 'g', 9));
        }
        key.extras = parts.join(QLatin1Char(';'));
    }
    return key;
}

// One cache per thread. Meter painting and the font tests run on the GUI
// thread. A full clear would drop the stable labels once numeric readouts
// filled the table, so a miss replaces the entry that has gone longest
// without a hit.
class ObjectTextFitCache {
public:
    static constexpr int kCap = 512;

    bool find(const ObjectTextFitKey& key, QFont* font)
    {
        const auto it = m_index.constFind(key);
        if (it == m_index.cend()) {
            return false;
        }
        m_slots[*it].stamp = ++m_clock;
        *font = m_slots[*it].font;
        return true;
    }

    void store(const ObjectTextFitKey& key, const QFont& font)
    {
        const auto existing = m_index.constFind(key);
        if (existing != m_index.cend()) {
            Slot& slot = m_slots[*existing];
            slot.font = font;
            slot.stamp = ++m_clock;
            return;
        }
        if (m_slots.size() < kCap) {
            m_index.insert(key, m_slots.size());
            m_slots.append(Slot{key, font, ++m_clock});
            return;
        }
        int oldest = 0;
        for (int i = 1; i < m_slots.size(); ++i) {
            if (m_slots[i].stamp < m_slots[oldest].stamp) {
                oldest = i;
            }
        }
        m_index.remove(m_slots[oldest].key);
        m_slots[oldest] = Slot{key, font, ++m_clock};
        m_index.insert(key, oldest);
    }

private:
    struct Slot {
        ObjectTextFitKey key;
        QFont font;
        quint64 stamp = 0;
    };
    QHash<ObjectTextFitKey, int> m_index;
    QVector<Slot> m_slots;
    quint64 m_clock = 0;
};

inline ObjectTextFitCache& objectTextFitCache()
{
    thread_local ObjectTextFitCache cache;
    return cache;
}

inline QFont fitObjectText(QFont font, double requested, const QString& text,
                           const QRectF& box, bool points=false, int flags=Qt::TextSingleLine, QPaintDevice* device=nullptr) {
    const ObjectTextFitKey key = objectTextFitKey(font, requested, text, box, points, flags, device);
    ObjectTextFitCache& cache = objectTextFitCache();
    QFont cached;
    if (cache.find(key, &cached)) {
        return cached;
    }
    const auto size=[&](double value) {
        if(points) {
            font.setPointSizeF(qMax(.1,value));
            // Point fonts resolve against the actual paint device. Preserve
            // their unit and exact size whenever they already meet the floor.
            font = WorkspacePreview::readableFont(font,device);
        }
        else { font.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(1,int(std::floor(value))))); }
    };
    size(requested);
    // Binary search final metrics, including bearings and wrapped lines.
    // Every candidate respects the workspace floor. Keep the original search
    // interval and pass count so already-readable fitted point sizes remain
    // bit-for-bit unchanged; an undersized box returns the minimum font.
    double low=0,high=qMax(.1,requested);
    const auto fits=[&]() {
        const QFontMetricsF metrics(font,device);
        const QRectF ink=metrics.boundingRect(box,flags,text);
        return metrics.height()<=box.height() && ink.width()<=box.width() && ink.height()<=box.height();
    };
    if(fits()) {
        cache.store(key, font);
        return font;
    }
    for(int pass=0;pass<16;++pass) {
        const double candidate=(low+high)/2; size(candidate);
        if(fits()) { low=candidate; } else { high=candidate; }
    }
    size(low);
    cache.store(key, font);
    return font;
}
// The text as drawn in `box` with `font`: whole when it fits, otherwise cut
// at the end with an ellipsis so it never spills past the box on either side.
inline QString elideObjectText(const QFont& font,const QString& text,const QRectF& box,
                               QPaintDevice* device=nullptr) {
    const QFontMetricsF metrics(font,device);
    if(metrics.horizontalAdvance(text)<=box.width()) { return text; }
    return metrics.elidedText(text,Qt::ElideRight,box.width());
}
// drawObjectText for a caption: fitted down to the readable minimum, then
// ended with an ellipsis if it still does not fit. Returns the text drawn.
inline QString drawObjectTextElided(QPainter& painter,const QRectF& box,const QString& text,
                                    double requested,int alignment=Qt::AlignCenter) {
    if(box.isEmpty() || text.isEmpty() || requested<=0) { return QString(); }
    const QFont font=fitObjectText(painter.font(),requested,text,box,false,Qt::TextSingleLine,painter.device());
    const QString shown=elideObjectText(font,text,box,painter.device());
    painter.setFont(font);
    painter.drawText(box,alignment,shown);
    return shown;
}
inline void drawObjectText(QPainter& painter,const QRectF& box,const QString& text,
                           double requested,int alignment=Qt::AlignCenter) {
    if(box.isEmpty() || text.isEmpty() || requested<=0) { return; }
    painter.setFont(fitObjectText(painter.font(),requested,text,box,false,Qt::TextSingleLine,painter.device()));
    painter.drawText(box,alignment,text);
}
}
