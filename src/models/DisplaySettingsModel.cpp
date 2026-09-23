// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-native model, no Thetis or AetherSDR
// equivalent. See DisplaySettingsModel.h for the full design note (3D
// Stacked-Trace Spectrum Plan Task 17).
//
// NereusSDR - DisplaySettingsModel implementation.
//
// Modification history (NereusSDR)
//   Created 2026-08-09 by J.J. Boyd / KG4VCF, 3D Stacked-Trace Spectrum
//     Plan Task 17. AI tooling: Claude Code.

#include "DisplaySettingsModel.h"

#include "core/AppSettings.h"

#include <QString>
#include <QStringLiteral>

#include <algorithm>

namespace NereusSDR {

namespace {

// Reimplements the file-local `settingsKey(base, panIndex)` helper in
// SpectrumWidget.cpp (that one is `static` to its own translation unit
// and not exported): pan 0 gets the bare key, every other pan gets a
// "_<panIndex>" suffix. Both copies must keep producing the SAME string
// for the SAME (base, panIndex) pair -- they read and write the identical
// AppSettings keys -- so any change to one belongs with the other.
QString settingsKeyFor(const QString& base, int panIndex)
{
    if (panIndex == 0) {
        return base;
    }
    return QStringLiteral("%1_%2").arg(base).arg(panIndex);
}

} // namespace

DisplaySettingsModel::DisplaySettingsModel(QObject* parent)
    : QObject(parent)
{
}

// ---- Waterfall ----

void DisplaySettingsModel::setWfColorScheme(int scheme)
{
    const int clamped = std::clamp(scheme, 0, 7); // WfColorScheme::Count == 8
    if (m_wfColorScheme == clamped) { return; }
    m_wfColorScheme = clamped;
    emit wfColorSchemeChanged(m_wfColorScheme);
}

void DisplaySettingsModel::setWfColorGain(int gain)
{
    const int clamped = std::clamp(gain, 0, 100);
    if (m_wfColorGain == clamped) { return; }
    m_wfColorGain = clamped;
    emit wfColorGainChanged(m_wfColorGain);
}

void DisplaySettingsModel::setWfBlackLevel(int level)
{
    const int clamped = std::clamp(level, 0, 125);
    if (m_wfBlackLevel == clamped) { return; }
    m_wfBlackLevel = clamped;
    emit wfBlackLevelChanged(m_wfBlackLevel);
}

// ---- Spectrum ----

void DisplaySettingsModel::setRefLevel(float dBm)
{
    // -180.0f..80.0f, not the popup slider's -160..20: SpectrumWidget's
    // Task 19 Ctrl-drag gesture (mouseMoveEvent's m_draggingDbmRange
    // branch) can legitimately drive refLevel up to upstream's own
    // kMaxDisplayDbm = 80.0f via clampDbmRangeForBottom -- found only
    // once this class was actually bound to SpectrumWidget (Task 18):
    // tst_dbm_range_drag.cpp's ctrlDragRange_clampsAtMaximum drags to
    // refLevel() == 60.0f, which a -160..20 model clamp would silently
    // narrow to 20.0f on the round trip back into the widget. Same
    // never-narrower-than-any-widget-write-path rule as Dyn Range above.
    const float clamped = std::clamp(dBm, -180.0f, 80.0f);
    if (qFuzzyCompare(m_refLevel + 1.0f, clamped + 1.0f)) { return; }
    m_refLevel = clamped;
    emit refLevelChanged(m_refLevel);
}

void DisplaySettingsModel::setDynamicRange(float dB)
{
    // 10.0f..200.0f, not the popup slider's 20..160: this is the widest
    // bound any widget write path accepts. From SpectrumWidget::wheelEvent
    // (wheel-over-dBm-strip block), qBound(10.0f, ..., 200.0f) -- the
    // model must never clamp narrower than that or a round trip through
    // the model would narrow a value the widget itself allows.
    const float clamped = std::clamp(dB, 10.0f, 200.0f);
    if (qFuzzyCompare(m_dynamicRange, clamped)) { return; }
    m_dynamicRange = clamped;
    emit dynamicRangeChanged(m_dynamicRange);
}

void DisplaySettingsModel::setFillAlpha(float alpha)
{
    const float clamped = std::clamp(alpha, 0.0f, 1.0f);
    // +1.0f offset: qFuzzyCompare's relative tolerance breaks down when
    // either operand is exactly (or very near) 0.0, which fillAlpha's
    // valid range legitimately includes -- same +1.0f idiom used by this
    // codebase's own tst_pan_display_settings_inherit.cpp for the same
    // reason.
    if (qFuzzyCompare(m_fillAlpha + 1.0f, clamped + 1.0f)) { return; }
    m_fillAlpha = clamped;
    emit fillAlphaChanged(m_fillAlpha);
}

void DisplaySettingsModel::setPanFill(bool on)
{
    if (m_panFill == on) { return; }
    m_panFill = on;
    emit panFillChanged(m_panFill);
}

void DisplaySettingsModel::setSpectrumFrac(float frac)
{
    const float clamped = std::clamp(frac, 0.10f, 0.90f);
    if (qFuzzyCompare(m_spectrumFrac, clamped)) { return; }
    m_spectrumFrac = clamped;
    emit spectrumFracChanged(m_spectrumFrac);
}

// ---- 3D ----

void DisplaySettingsModel::setSpectrumRenderMode(int mode)
{
    const int clamped = std::clamp(mode, 0, 1); // SpectrumRenderMode::Count == 2
    if (m_spectrumRenderMode == clamped) { return; }
    m_spectrumRenderMode = clamped;
    emit spectrumRenderModeChanged(m_spectrumRenderMode);
}

void DisplaySettingsModel::setDssFloorDepth(int depthDb)
{
    const int clamped = std::clamp(depthDb, 0, 24);
    if (m_dssFloorDepth == clamped) { return; }
    m_dssFloorDepth = clamped;
    emit dssFloorDepthChanged(m_dssFloorDepth);
}

void DisplaySettingsModel::setDssGain(int pct)
{
    const int clamped = std::clamp(pct, 0, 100);
    if (m_dssGain == clamped) { return; }
    m_dssGain = clamped;
    emit dssGainChanged(m_dssGain);
}

void DisplaySettingsModel::setDssRowSpan(int pct)
{
    const int clamped = std::clamp(pct, 0, 100);
    if (m_dssRowSpan == clamped) { return; }
    m_dssRowSpan = clamped;
    emit dssRowSpanChanged(m_dssRowSpan);
}

void DisplaySettingsModel::setDssAngle(int pct)
{
    const int clamped = std::clamp(pct, 0, 100);
    if (m_dssAngle == clamped) { return; }
    m_dssAngle = clamped;
    emit dssAngleChanged(m_dssAngle);
}

void DisplaySettingsModel::setDssRowDivider(int n)
{
    const int clamped = std::clamp(n, 0, 10);
    if (m_dssRowDivider == clamped) { return; }
    m_dssRowDivider = clamped;
    emit dssRowDividerChanged(m_dssRowDivider);
}

void DisplaySettingsModel::setThreeDSliceDepth(bool on)
{
    if (m_threeDSliceDepth == on) { return; }
    m_threeDSliceDepth = on;
    emit threeDSliceDepthChanged(m_threeDSliceDepth);
}

// ---- Persistence ----

void DisplaySettingsModel::load()
{
    auto& s = AppSettings::instance();

    // Pan-0-fallback inheritance: an untouched pan N reads pan 0's value
    // until it has one of its own. Mirrors SpectrumWidget::loadSettings()'s
    // rawValue lambda exactly (2026-07-30 bench fix; see that function's
    // own comment for the full rationale) -- both must keep agreeing on
    // this, since they read the same keys for the same operator-visible
    // behaviour.
    auto rawValue = [&](const QString& key) -> QString {
        const QString own = s.value(settingsKeyFor(key, m_panIndex)).toString();
        if (!own.isEmpty() || m_panIndex == 0) { return own; }
        return s.value(settingsKeyFor(key, 0)).toString();
    };
    auto readFloat = [&](const QString& key, float def) -> float {
        const QString val = rawValue(key);
        if (val.isEmpty()) { return def; }
        bool ok = false;
        const float v = val.toFloat(&ok);
        return ok ? v : def;
    };
    auto readInt = [&](const QString& key, int def) -> int {
        const QString val = rawValue(key);
        if (val.isEmpty()) { return def; }
        bool ok = false;
        const int v = val.toInt(&ok);
        return ok ? v : def;
    };
    auto readBool = [&](const QString& key, bool def) -> bool {
        const QString val = rawValue(key);
        if (val.isEmpty()) { return def; }
        return val == QStringLiteral("True");
    };

    // Defaults match SpectrumWidget::loadSettings()'s ship defaults for
    // the same keys exactly, so a fresh install looks identical through
    // either object.
    setWfColorScheme(readInt(QStringLiteral("DisplayWfColorScheme"), 0));
    setWfColorGain(readInt(QStringLiteral("DisplayWfColorGain"), 45));
    setWfBlackLevel(readInt(QStringLiteral("DisplayWfBlackLevel"), 104));

    const float gridMax = readFloat(QStringLiteral("DisplayGridMax"), -48.0f);
    const float gridMin = readFloat(QStringLiteral("DisplayGridMin"), -116.0f);
    setRefLevel(gridMax);
    setDynamicRange(gridMax - gridMin);

    setFillAlpha(readFloat(QStringLiteral("DisplayFftFillAlpha"), 0.70f));
    setPanFill(readBool(QStringLiteral("DisplayPanFill"), true));
    setSpectrumFrac(readFloat(QStringLiteral("DisplaySpectrumFrac"), 0.40f));

    setSpectrumRenderMode(readInt(QStringLiteral("DisplaySpectrumRenderMode"), 0));
    setDssGain(readInt(QStringLiteral("Display3DGain"), 70));
    setDssRowSpan(readInt(QStringLiteral("Display3DSpan"), 100));
    setDssAngle(readInt(QStringLiteral("Display3DAngle"), 50));
    setDssRowDivider(readInt(QStringLiteral("Display3DSpeed"), 0));
    setThreeDSliceDepth(readBool(QStringLiteral("Display3DSliceShadow"), false));
    // dssFloorDepth is deliberately not read here: it is not scoped by
    // panIndex() at all, and its persistence lives entirely on
    // PanadapterModel, per band (see the file header).
}

void DisplaySettingsModel::save()
{
    auto& s = AppSettings::instance();

    auto writeFloat = [&](const QString& key, float val) {
        s.setValue(settingsKeyFor(key, m_panIndex), QString::number(static_cast<double>(val)));
    };
    auto writeInt = [&](const QString& key, int val) {
        s.setValue(settingsKeyFor(key, m_panIndex), QString::number(val));
    };
    auto writeBool = [&](const QString& key, bool val) {
        s.setValue(settingsKeyFor(key, m_panIndex),
                   val ? QStringLiteral("True") : QStringLiteral("False"));
    };

    writeInt(QStringLiteral("DisplayWfColorScheme"), m_wfColorScheme);
    writeInt(QStringLiteral("DisplayWfColorGain"), m_wfColorGain);
    writeInt(QStringLiteral("DisplayWfBlackLevel"), m_wfBlackLevel);

    // Same derived-key shape as SpectrumWidget::saveSettings(): refLevel
    // is stored directly as DisplayGridMax; dynamicRange is stored as the
    // computed DisplayGridMin (refLevel - dynamicRange), not as its own
    // key -- there is no "DisplayGridDynamicRange" key anywhere in this
    // codebase.
    writeFloat(QStringLiteral("DisplayGridMax"), m_refLevel);
    writeFloat(QStringLiteral("DisplayGridMin"), m_refLevel - m_dynamicRange);

    writeFloat(QStringLiteral("DisplayFftFillAlpha"), m_fillAlpha);
    writeBool(QStringLiteral("DisplayPanFill"), m_panFill);
    writeFloat(QStringLiteral("DisplaySpectrumFrac"), m_spectrumFrac);

    writeInt(QStringLiteral("DisplaySpectrumRenderMode"), m_spectrumRenderMode);
    writeInt(QStringLiteral("Display3DGain"), m_dssGain);
    writeInt(QStringLiteral("Display3DSpan"), m_dssRowSpan);
    writeInt(QStringLiteral("Display3DAngle"), m_dssAngle);
    writeInt(QStringLiteral("Display3DSpeed"), m_dssRowDivider);
    writeBool(QStringLiteral("Display3DSliceShadow"), m_threeDSliceDepth);
    // dssFloorDepth is deliberately not written here: same reason as the
    // matching comment in load() above.
}

} // namespace NereusSDR
