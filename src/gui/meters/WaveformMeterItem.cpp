// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-10-26 - Colours at their defaults follow the workspace look when
//                 drawn (ItemColourLook.h); saved colours are unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-11  J.J. Boyd / KG4VCF; original waveform observation. AI tooling: OpenAI Codex.
// 2026-10-14  J.J. Boyd / KG4VCF; widen normalized geometry before pixel scaling. AI tooling: OpenAI Codex.
// no-port-check: Original native painter; FreeDV fixed amplitude/time plot facts
// studied at freedv-gui a8d8424 main.cpp:1377-1382, defines.h:54-56.
#include "WaveformMeterItem.h"
#include "ItemColourLook.h"
#include "WaveformMeterStyle.h"
#include <QJsonDocument>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
namespace NereusSDR {
WaveformMeterItem::WaveformMeterItem(Source source, QObject* parent) : MeterItem(parent), m_source(source)
{
    setRect(0, 0, 1, 1);
    m_config = {{"schema", 1},
                {"kind", typeId()},
                {"title", source == Source::Microphone ? "Microphone waveform" : "Decoded speech waveform"},
                {"showTitle", true},
                {"viewWindowMs", 5000},
                {"faceHeight", 300},
                {"traceColor", "#7fae91"},
                {"sourceStage", "Conditioned"}};
}
QString WaveformMeterItem::typeId() const
{
    return m_source == Source::Microphone ? "meter.radeMicWaveform" : "meter.radeSpeechWaveform";
}
QJsonObject WaveformMeterItem::configuration() const
{
    QJsonObject config = m_config;
    config["x"] = x();
    config["y"] = y();
    config["w"] = itemWidth();
    config["h"] = itemHeight();
    return config;
}
bool WaveformMeterItem::applyConfiguration(const QJsonObject& config)
{
    QJsonObject candidate = m_config;
    for (auto it = config.begin(); it != config.end(); ++it) {
        candidate[it.key()] = it.value();
    }
    if (candidate.value("kind").toString() != typeId() || candidate.value("schema").toInt() != 1) {
        return false;
    }
    const int window = candidate.value("viewWindowMs").toInt(-1);
    if (window != 5000 && window != 1000 && window != 100) {
        return false;
    }
    const QString stage = candidate.value("sourceStage").toString();
    if (stage != "Conditioned" && stage != "RawMic") {
        return false;
    }
    if (m_source == Source::DecodedSpeech && stage != "Conditioned") {
        return false;
    }
    const int height = candidate.value("faceHeight").toInt();
    if (height < 180 || height > 4000) {
        return false;
    }
    for (const QString& key :
         {QStringLiteral("traceColor"), QStringLiteral("gridColor"), QStringLiteral("textColor")}) {
        if (candidate.contains(key) &&
            (!candidate[key].isString() || !QColor(candidate[key].toString()).isValid())) {
            return false;
        }
    }
    for (const QString& key :
         {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("w"), QStringLiteral("h")}) {
        if (candidate.contains(key) &&
            (!candidate[key].isDouble() || !std::isfinite(candidate[key].toDouble()) ||
             candidate[key].toDouble() < 0)) {
            return false;
        }
    }
    if (candidate.contains("w") && candidate["w"].toDouble() <= 0) {
        return false;
    }
    if (candidate.contains("h") && candidate["h"].toDouble() <= 0) {
        return false;
    }
    if (!candidate["title"].isString() || !candidate["showTitle"].isBool()) {
        return false;
    }
    const WaveformKind previous = waveformKind();
    setRect(candidate["x"].toDouble(x()), candidate["y"].toDouble(y()), candidate["w"].toDouble(itemWidth()),
            candidate["h"].toDouble(itemHeight()));
    if (candidate != m_config) {
        m_staticDirty = true;
    }
    m_config = candidate;
    if (previous != waveformKind()) {
        m_snapshot.reset();
    }
    return true;
}
QString WaveformMeterItem::serialize() const
{
    return QString::fromUtf8(QJsonDocument(configuration()).toJson(QJsonDocument::Compact));
}
bool WaveformMeterItem::deserialize(const QString& record)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(record.toUtf8(), &error);
    return error.error == QJsonParseError::NoError && doc.isObject() && applyConfiguration(doc.object());
}
WaveformKind WaveformMeterItem::waveformKind() const
{
    return m_source == Source::DecodedSpeech                ? WaveformKind::DecodedSpeech
           : m_config["sourceStage"].toString() == "RawMic" ? WaveformKind::RawMicrophone
                                                            : WaveformKind::ConditionedMicrophone;
}
bool WaveformMeterItem::setSnapshot(WaveformSnapshotPtr snapshot)
{
    if (m_snapshot == snapshot) {
        return false;
    }
    m_snapshot = std::move(snapshot);
    return true;
}
void WaveformMeterItem::paint(QPainter& painter, int width, int height)
{
    paintForLayer(painter, width, height, Layer::Background);
    paintForLayer(painter, width, height, Layer::OverlayDynamic);
}
void WaveformMeterItem::paintForLayer(QPainter& painter, int width, int height, Layer layer)
{
    using namespace WaveformMeterStyle;
    const QRectF face(qreal(x()) * width, qreal(y()) * height,
                      qreal(itemWidth()) * width, qreal(itemHeight()) * height);
    const QRectF plot = face.adjusted(48, 38, -16, -42);
    if (plot.width() <= 0 || plot.height() <= 0) {
        return;
    }
    painter.save();
    painter.setClipRect(face);
    QFont font = painter.font();
    font.setPixelSize(11);
    painter.setFont(font);
    if (layer == Layer::Background) {
        painter.fillRect(face, panel());
        painter.fillRect(plot, inset());
        painter.setPen(border());
        painter.drawRect(plot);
        const QColor gridColor =
            m_config.contains("gridColor") ? QColor(m_config["gridColor"].toString()) : grid();
        for (int tick = 0; tick <= 10; ++tick) {
            const qreal yy = plot.top() + plot.height() * tick / 10;
            painter.setPen(gridColor);
            painter.drawLine(QPointF(plot.left(), yy), QPointF(plot.right(), yy));
            painter.setPen(scale());
            painter.drawText(QRectF(face.left() + 2, yy - 7, 40, 14), Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(double(5 - tick) / 5., 'f', 1));
        }
        const int divisions = viewWindowMs() == 5000 ? 5 : viewWindowMs() == 1000 ? 5 : 10;
        for (int tick = 0; tick <= divisions; ++tick) {
            const qreal xx = plot.left() + plot.width() * tick / divisions;
            painter.setPen(gridColor);
            painter.drawLine(QPointF(xx, plot.top()), QPointF(xx, plot.bottom()));
            painter.setPen(scale());
            const double remaining = double(viewWindowMs()) * (divisions - tick) / divisions;
            const QString label = viewWindowMs() == 100 ? QString::number(int(remaining)) + " ms"
                                                        : QString::number(remaining / 1000., 'g', 2) + " s";
            painter.drawText(QRectF(xx - 24, plot.bottom() + 3, 48, 16), Qt::AlignCenter, label);
        }
        if (m_config["showTitle"].toBool()) {
            painter.setPen(m_config.contains("textColor") ? QColor(m_config["textColor"].toString())
                                                          : text());
            font.setPixelSize(13);
            painter.setFont(font);
            painter.drawText(face.adjusted(16, 8, -16, -face.height() + 31), Qt::AlignLeft | Qt::AlignVCenter,
                             m_config["title"].toString());
        }
    } else if (layer == Layer::OverlayDynamic) {
        QString caption = waveformKind() == WaveformKind::RawMicrophone ? "Raw microphone · 48 kHz"
                          : waveformKind() == WaveformKind::ConditionedMicrophone
                              ? "Conditioned encoder input · 16 kHz"
                              : "Decoded speech · 24 kHz";
        const bool available = m_snapshot && m_snapshot->availability == WaveformAvailability::Available &&
                               !m_snapshot->samples.isEmpty() && m_snapshot->sampleRate > 0;
        bool clipped = false;
        if (available) {
            painter.save();
            painter.setClipRect(plot);
            painter.setRenderHint(QPainter::Antialiasing, false);
            QPen tracePen(ItemColourLook::effective("Waveform.traceColor", QColor(m_config["traceColor"].toString()), this), 1);
            tracePen.setCosmetic(true);
            painter.setPen(tracePen);
            const qreal dpr = std::max<qreal>(1, painter.device()->devicePixelRatioF());
            const int columns = std::max(1, int(std::ceil(plot.width() * dpr)));
            const qint64 start = m_snapshot->asOfNs - qint64(viewWindowMs()) * 1000000;
            const double duration = double(viewWindowMs()) * 1000000;
            const auto yFor = [&](float sample) {
                clipped = clipped || std::abs(sample) > 1;
                return plot.center().y() - std::clamp(sample, -1.f, 1.f) * plot.height() / 2;
            };
            for (const WaveformSegment& segment : m_snapshot->segments) {
                if (segment.offset < 0 || segment.count <= 0 ||
                    segment.offset + segment.count > m_snapshot->samples.size()) {
                    continue;
                }
                QPainterPath path;
                bool pathStarted = false;
                int currentColumn = -1;
                float minimum = 0, maximum = 0;
                bool binned = false;
                const auto flush = [&] {
                    if (binned) {
                        // A fractional canvas width can leave a partial final column;
                        // keep its extrema inside the physical clipping boundary.
                        const qreal xx = std::min(plot.left() + (currentColumn + .5) / dpr,
                                                  plot.right() - .5 / dpr);
                        painter.drawLine(QPointF(xx, yFor(minimum)), QPointF(xx, yFor(maximum)));
                    }
                };
                const double sampleWidth = plot.width() * 1000000000. / (duration * m_snapshot->sampleRate);
                for (int i = 0; i < segment.count; ++i) {
                    const qint64 time = segment.startNs + qint64(i) * 1000000000LL / m_snapshot->sampleRate;
                    if (time < start || time > m_snapshot->asOfNs) {
                        continue;
                    }
                    const qreal xx = plot.left() + double(time - start) / duration * plot.width();
                    const float sample = m_snapshot->samples[segment.offset + i];
                    if (!std::isfinite(sample)) {
                        flush();
                        binned = false;
                        pathStarted = false;
                        continue;
                    }
                    if (sampleWidth * dpr >= .5) {
                        const QPointF point(xx, yFor(sample));
                        if (pathStarted) {
                            path.lineTo(point);
                        } else {
                            path.moveTo(point);
                            pathStarted = true;
                        }
                        painter.drawPoint(point);
                    } else {
                        const int column = std::clamp(int((xx - plot.left()) * dpr), 0, columns - 1);
                        if (!binned || column != currentColumn) {
                            flush();
                            currentColumn = column;
                            minimum = maximum = sample;
                            binned = true;
                        } else {
                            minimum = std::min(minimum, sample);
                            maximum = std::max(maximum, sample);
                        }
                    }
                }
                flush();
                painter.drawPath(path);
            }
            painter.restore();
            if (clipped) {
                caption += " · clipped";
            }
            if (m_snapshot->droppedFrames) {
                caption += " · observation gaps";
            }
        } else {
            const QString reason = m_snapshot && !m_snapshot->reason.isEmpty()
                                       ? m_snapshot->reason
                                       : QStringLiteral("Waiting for observed PCM");
            painter.setPen(secondary());
            painter.drawText(plot.adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, reason);
        }
        painter.setPen(secondary());
        painter.drawText(QRectF(face.left() + 48, face.bottom() - 20, face.width() - 64, 16),
                         Qt::AlignLeft | Qt::AlignVCenter, caption);
    }
    painter.restore();
}
} // namespace NereusSDR
