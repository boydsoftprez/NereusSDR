// no-port-check: NereusSDR-original. Maps each container button to the
// NereusSDR feature it fronts, on the container's own slice.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/containers/ContainerButtonDispatcher.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port.
//
// R-R3-49 / R-R3-21: a container's function buttons (OtherButtonItem) and
// its band buttons do what their labels say. Each connected button calls
// the NereusSDR target the rest of the app already uses (the same setters
// the TX applet, the RX applet, the VFO flag, the pan overlay and Setup
// call), so a remote window reaches the Core exactly as those do.
//
// Buttons that act on a receiver act on the container's own slice
// (ContainerWidget::rxSource(), slices A to D), never on the active slice:
// a container set to slice A acts on slice A while slice B is active. A
// container set to a slice that is not open shows those buttons
// unavailable, with the reason, and a click changes nothing.
//
// Transmit buttons (MON, TUN, MOX, 2TON, PS-A) work as the TX applet's do
// with a radio connected here. With no radio, TUN, MOX and 2TON are
// unavailable. In a remote window they show the transmit reason and change
// nothing (remote transmit comes later), except MON, a transmit setting
// that keys nothing: it toggles the Core's MON while the Core takes
// transmit settings and its radio is off the air (R-R3-49).
//
// The dispatcher holds no state of its own: every lit state is read from
// the target each time apply() runs.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-49, R-R3-21). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan, desktop remote
//                                    transmit (R-IOS-13): the Core's reason
//                                    now (remoteTransmitReasonNow).
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2): MON follows
//                                    the transmit settings gate in a
//                                    remote window (the Core's monEnabled).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  A11 / R-R3-49 (parity Task 31): the
//                                    DUP hooks (display duplex).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 15 fix round
//                                    1: the sliceRefusal hook refuses a
//                                    slice another device controls with
//                                    the RX applet's reason, in remote and
//                                    hosting windows. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  The container Power button is removed
//                                    (maintainer decision): no Power hooks.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "gui/meters/OtherButtonItem.h"

#include <QString>

#include <functional>

namespace NereusSDR {

class AudioEngine;
class BandButtonItem;
class ButtonBoxItem;
class RadioModel;
class SliceModel;
class SpectrumWidget;

class ContainerButtonDispatcher {
public:
    using Id = OtherButtonItem::ButtonId;

    // What the window supplies: the parts that live on MainWindow.
    struct Hooks {
        // Present only while this local window hosts the Core.
        std::function<bool()> desktopHosting;
        // Slice control plan Task 15 fix round 1: why this window may not
        // change a slice (one another device controls, the RX applet's
        // reason), empty when it may. Local and remote windows alike.
        std::function<QString(int sliceId)> sliceRefusal;
        std::function<bool()> desktopMoxOn;
        std::function<bool()> desktopTuneOn;
        std::function<void(bool)> requestDesktopMox;
        std::function<void(bool)> requestDesktopTune;
        // Fix wave (hosting 2-TONE parity): 2TONE in a hosting window.
        std::function<void(bool)> requestDesktopTwoTone;
        // Remote windows: whether the Core takes this window's transmit
        // controls, and the reason shown when it does not.
        std::function<bool()> transmitPermitted;
        QString remoteTransmitReason;
        // Desktop remote transmit (R-IOS-13): the Core's own reason now,
        // when set; preferred over remoteTransmitReason.
        std::function<QString()> remoteTransmitReasonNow;
        // R-R3-49 (parity Task 2): remote windows, whether the Core takes
        // this window's transmit settings now (off the air), and why not.
        std::function<bool()> transmitSettingsPermitted;
        std::function<QString()> transmitSettingsReason;
        // R-R3-49 (parity Task 7): remote windows, whether the Core takes
        // this window's PureSignal arming now (PS-A), and why not.
        std::function<bool()> pureSignalArmingPermitted;
        std::function<QString()> pureSignalArmingReason;
        // The panadapter that shows a slice (Peak, CTUN).
        std::function<SpectrumWidget*(SliceModel*)> spectrumFor;
        // This computer's VAX outputs (VAX 1, VAX 2). May be null.
        AudioEngine* vaxDevices{nullptr};
        // Parity Task 31 (A11): DUP, the window's DisplayDuplex setting:
        // whether it is on, the click, and why it cannot change (a remote
        // window on a Core below txDisplayVersion 3), empty when it can.
        std::function<bool()> displayDuplexOn;
        std::function<void(bool)> setDisplayDuplex;
        std::function<QString()> displayDuplexReason;
    };

    struct State {
        bool on{false};
        bool available{true};
        QString reason;  // why it is unavailable, in plain words
    };

    ContainerButtonDispatcher(RadioModel* model, Hooks hooks);

    // The container's slice, or null when that slice is not open.
    SliceModel* sliceFor(int rxSource) const;
    // Why a container set to a slice that is not open does nothing.
    static QString noSliceReason(int rxSource);

    // The lit and available state of one function button.
    State stateOf(Id id, int rxSource) const;
    // Push every function button's state into `item`.
    void apply(OtherButtonItem* item, int rxSource) const;
    // Act on a click. Returns an empty string when it acted, otherwise the
    // plain reason it changed nothing.
    QString click(Id id, int rxSource);

    // Band, mode, filter, antenna and tune step boxes: every button
    // available while the container's slice is open, otherwise unavailable
    // with noSliceReason().
    void applySliceAvailability(ButtonBoxItem* box, int rxSource) const;
    // Light the band of the container's slice (none when it is not open).
    void applyBand(BandButtonItem* item, int rxSource) const;
    // A band button click: that band on the container's own slice.
    QString clickBand(int bandUiIndex, int rxSource);

    // Plain reasons, exposed for tests.
    static QString noRadioTransmitReason();

private:
    QString sliceUnavailableReason(int rxSource) const;
    bool transmitBlockedRemotely() const;
    SpectrumWidget* spectrumOf(int rxSource) const;

    RadioModel* m_model{nullptr};
    Hooks m_hooks;
};

} // namespace NereusSDR
