#pragma once

// =================================================================
// src/core/ReceiverManager.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include <QObject>
#include <QVector>
#include <QMap>

#include "codec/CodecContext.h"   // PsDdcConfig + Q_DECLARE_METATYPE
#include "HpsdrModel.h"           // HPSDRModel

namespace NereusSDR {

class IP1Codec;
class IP2Codec;

// Per-receiver configuration state.
struct ReceiverConfig {
    int receiverIndex{-1};       // Logical receiver index (0-based)
    int hardwareRx{-1};          // Hardware DDC index (e.g., DDC2 for ANAN-G2 RX1)
    int wdspChannel{-1};         // WDSP channel number (assigned at creation)
    int adcIndex{0};             // Which ADC feeds this receiver's DDC (0 or 1)
    int ddcIndex{-1};            // Explicit DDC mapping (-1 = auto-assign)
    quint64 frequencyHz{14225000};
    int sampleRate{48000};
    bool active{false};
    bool isDiversity{false};     // Sub-receiver for diversity combining
    bool isPureSignalFeedback{false}; // Dedicated to PA linearization
};

// Client-side receiver lifecycle management.
// Maps logical receivers to hardware DDC channels and (future) WDSP instances.
// Lives on the main thread; RadioModel owns it.
class ReceiverManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(int activeReceiverCount READ activeReceiverCount
               NOTIFY activeReceiverCountChanged)

public:
    explicit ReceiverManager(QObject* parent = nullptr);
    ~ReceiverManager() override;

    // --- Configuration ---
    void setMaxReceivers(int max);
    int maxReceivers() const { return m_maxReceivers; }

    // --- Receiver Lifecycle ---
    // Create a new receiver. Returns the receiver index, or -1 if at max.
    int createReceiver();

    // Destroy a receiver and release its hardware DDC.
    void destroyReceiver(int receiverIndex);

    // Drop all receivers and reset the manager to a just-constructed state.
    // Called by RadioModel::teardownConnection so the next connectToRadio
    // starts with a fresh index counter; without this, a same-session
    // disconnect/reconnect leaks receiver 0 and the new createReceiver()
    // returns 1, colliding on DDC2 for P2 2-ADC boards (issue #75).
    void reset();

    // Activate/deactivate a receiver.
    // Activating increments the hardware receiver count sent to the radio.
    void activateReceiver(int receiverIndex);
    void deactivateReceiver(int receiverIndex);

    // --- State Queries ---
    int activeReceiverCount() const;
    bool isReceiverActive(int receiverIndex) const;
    ReceiverConfig receiverConfig(int receiverIndex) const;

    // --- DDC frequency lock (CTUN mode) ---
    // When locked, setReceiverFrequency stores the freq but does NOT
    // emit hardwareFrequencyChanged. MainWindow manages DDC directly.
    void setDdcFrequencyLocked(bool locked) { m_ddcFreqLocked = locked; }
    bool ddcFrequencyLocked() const { return m_ddcFreqLocked; }

    // --- Receiver Configuration ---
    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz);
    // Force DDC retune even when locked (used by MainWindow CTUN pan drag)
    void forceHardwareFrequency(int receiverIndex, quint64 frequencyHz);
    void setReceiverSampleRate(int receiverIndex, int sampleRate);

    // Set explicit DDC mapping for a receiver.
    // For ANAN-G2: receiver 0 → DDC 2 (from Thetis UpdateDDCs console.cs:8216).
    // If ddcIndex = -1, sequential auto-assignment is used.
    void setDdcMapping(int receiverIndex, int ddcIndex);
    int ddcIndex(int receiverIndex) const;

    // Set which ADC feeds this receiver's DDC (for 2-ADC boards).
    void setAdcForReceiver(int receiverIndex, int adcIndex);

    // --- I/Q Data Routing ---
    // Called when I/Q data arrives for a hardware DDC.
    // Routes to the correct logical receiver.
    void feedIqData(int hwReceiverIndex, const QVector<float>& samples);

    // -------------------------------------------------------------------
    // Phase 3M-4 Task 6: PureSignal DDC orchestration
    //
    // Mirrors the per-board switch in Thetis console.cs:8186-8538
    // UpdateDDCs() [v2.10.3.13] (and mi0bot console.cs:8409-8488
    // [v2.10.3.13-beta2] for HL2). State-shadowing setters drive the
    // codec dispatch; the codec layer (Task 5) returns the wire-byte
    // PsDdcConfig and ReceiverManager re-emits it via ddcConfigChanged
    // for RadioConnection to consume (the wire-byte writer wiring lands
    // in Task 7 / a Phase-3F refactor).
    //
    // Codec injection is via setter (not constructor) because:
    //   1. ReceiverManager is constructed in RadioModel's ctor before
    //      the connected radio's protocol/board is known.  The codec
    //      selection happens later when applyHardwareInfo() runs.
    //   2. Two separate setters (P1 / P2) keep the IP1Codec / IP2Codec
    //      type split clean — RadioModel only ever wires one based on
    //      the protocol, and the other stays nullptr.
    //   3. Codec lifetime is owned by the connection (P1/P2RadioConnection
    //      hold std::unique_ptr<I*Codec>).  ReceiverManager stores a raw
    //      pointer with non-owning semantics and clears it on reset()
    //      (RadioModel::teardownConnection clears the codec ptr first).
    //
    // The setters are non-owning observers; the caller retains lifetime
    // ownership.  Pass nullptr to clear (called from reset()).
    void setP1Codec(IP1Codec* codec);
    void setP2Codec(IP2Codec* codec);

    // The connected radio's HPSDRModel — required because the codec layer
    // dispatches on this enum (e.g. P1CodecStandard maps multiple HpsdrModel
    // values to distinct UpdateDDCs branches).
    void setHpsdrModel(HPSDRModel model);
    HPSDRModel hpsdrModel() const noexcept { return m_hpsdrModel; }

    // PureSignal master enable (TX-side).  When true and MOX is also true,
    // the per-board codec emits the PS-on DDC layout (G2: DDC0+DDC2 with
    // ps_rate=192000; HL2: DDC0 with rx1_rate, etc.).
    void setPureSignalEnabled(bool on);

    // MOX (transmit) state.  PS DDC layout differs MOX-on vs MOX-off
    // (cal armed but TX off → DDC2 RX-only; MOX → full PS dual-stream).
    void setMox(bool on);

    // Diversity master enable.  Combines with PS in some board branches
    // (G2-class same as no-diversity per console.cs:8267-8277, but other
    // branches differ).
    void setDiversityEnabled(bool on);

    // RX1 / RX2 sample rates (Hz).  HL2 uses rx1_rate as PS feedback rate;
    // G2-class uses ps_rate=192000 regardless.
    void setRx1Rate(int rateHz);
    void setRx2Rate(int rateHz);

    // RX2 enable.  When true, additional DDC (DDC3 on G2-class, DDC1 on
    // HL2 PS-off path) is added to ddcEnable.
    void setRx2Enabled(bool on);

    // ADC control register shadows.  G2-class PS-on cntrl1 formula is
    // (rx_adc_ctrl1 & 0xf3) | 0x08 — preserves caller's bits 0,1,4..7,
    // overrides bits 2-3 with 0x08.  cntrl2 is masked similarly per-codec.
    void setRxAdcCtrl1(quint8 reg);
    void setRxAdcCtrl2(quint8 reg);

signals:
    void activeReceiverCountChanged(int count);
    void receiverCreated(int receiverIndex);
    void receiverDestroyed(int receiverIndex);
    void receiverActivated(int receiverIndex);
    void receiverDeactivated(int receiverIndex);
    void receiverFrequencyChanged(int receiverIndex, quint64 frequencyHz);

    // Request RadioConnection to update hardware state.
    // RadioModel wires these to RadioConnection slots.
    void hardwareReceiverCountChanged(int count);
    void hardwareFrequencyChanged(int hardwareRx, quint64 frequencyHz);

    // I/Q data routed to the appropriate WDSP channel.
    void iqDataForChannel(int wdspChannel, const QVector<float>& samples);

    // I/Q data routed to the appropriate receiver (by logical index).
    void iqDataForReceiver(int receiverIndex, const QVector<float>& samples);

    // Phase 3M-4 Task 6: emitted whenever ReceiverManager re-runs the
    // per-board PS DDC computation (either codec dispatch).  Carries the
    // wire-byte map; the consumer (RadioConnection follow-up) writes it
    // to the radio via NetworkIO.EnableRxs / SetDDCRate / SetADC_cntrl*
    // (per Thetis console.cs:8527-8534 [v2.10.3.13]).
    void ddcConfigChanged(const NereusSDR::PsDdcConfig& config);

private:
    // Rebuild hardware DDC mapping after receiver changes.
    void rebuildHardwareMapping();

    // Phase 3M-4 Task 6: re-runs the per-board PS DDC computation by
    // dispatching to the injected codec's applyPureSignalDdcConfig() and
    // re-emitting the result.  Called from every state setter on actual
    // change.  No-op when no codec is injected (graceful pre-connect path).
    //
    // Source: orchestration of Thetis console.cs:8186-8538 UpdateDDCs()
    // [v2.10.3.13]; the per-board switch lives in the codec layer.
    void updateDdcAssignment();

    int m_maxReceivers{7};
    int m_nextWdspChannel{0};
    bool m_ddcFreqLocked{false};

    // Receivers keyed by logical index
    QMap<int, ReceiverConfig> m_receivers;

    // Mapping from hardware DDC index to logical receiver index
    QMap<int, int> m_hwToLogical;

    // Diagnostic: one-shot logging of first successful and first dropped feedIqData
    bool m_firstForwardLogged{false};
    bool m_firstDropLogged{false};

    // -------------------------------------------------------------------
    // Phase 3M-4 Task 6: PureSignal DDC orchestration state
    //
    // Non-owning codec pointers — set by RadioModel post-connect via
    // setP1Codec / setP2Codec; cleared by reset().  P2 takes precedence
    // when both are non-null (defensive — RadioModel only sets one).
    IP1Codec* m_p1Codec{nullptr};
    IP2Codec* m_p2Codec{nullptr};

    // Connected radio's HPSDRModel — required by codec dispatch.
    HPSDRModel m_hpsdrModel{HPSDRModel::HPSDR};

    // Shadow of last-emitted live state.  Setters compare against these
    // and skip the codec dispatch when the new value matches (idempotence).
    bool   m_psEnabled{false};
    bool   m_moxState{false};
    bool   m_diversityEnabled{false};
    int    m_rx1Rate{48000};
    int    m_rx2Rate{48000};
    bool   m_rx2Enabled{false};
    quint8 m_rxAdcCtrl1{0};
    quint8 m_rxAdcCtrl2{0};

    static const ReceiverConfig kInvalidConfig;
};

} // namespace NereusSDR
