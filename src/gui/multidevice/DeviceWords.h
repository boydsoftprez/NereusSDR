#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/DeviceWords.h  (NereusSDR)
// =================================================================
//
// The words the desktop's several-devices screens share (iPhone app plan
// Task 78, R-IOS-02, R-IOS-07, R-IOS-30; the several-devices design,
// section 12). Plain operator English, following the phone's screens:
// a slice by its letter and frequency, a receiver as "Receiver n", a
// duration in minutes and hours, who holds transmit.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-02, R-IOS-07,
//               R-IOS-30), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QJsonObject>
#include <QString>

namespace NereusSDR {

class TransmitState;
struct RemoteDeviceSlice;

namespace DeviceWords {

/// "14.200 MHz".
QString frequency(double hz);
/// "USB" from a slice's dspMode value; empty for an unknown one.
QString mode(int dspMode);
/// "20m" from a Band value; empty for an unknown one.
QString band(int band);
/// "Receiver 2" for streamIndex 1; empty for -1.
QString receiver(int streamIndex);
/// "under a minute", "5 minutes", "1 hour 5 minutes".
QString duration(qint64 seconds);
/// "B 20m" for a slice in a device's list.
QString sliceShort(const RemoteDeviceSlice& slice);
/// A slice named in a question or notice ({sliceId, letter, frequencyHz,
/// mode, band}): "Slice B, 14.200 MHz USB".
QString sliceLine(const QJsonObject& slice);

/// The bottom banner's words for who holds transmit (section 12 item 2).
struct HolderBadge {
    enum class Tone { Listening, OnAir, Away, ChangingHands };
    /// False while nobody else holds transmit (nobody, or this window).
    bool shown = false;
    QString label;
    Tone tone = Tone::Listening;
    QString toolTip;
};
/// From the Core's txState, for the window whose device id is `selfId`.
HolderBadge holderBadge(const TransmitState& tx, const QString& selfId);

} // namespace DeviceWords
} // namespace NereusSDR
