// =================================================================
// src/models/RadioModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIO.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
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

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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
//=================================================================
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

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
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

//
// Upstream source 'Project Files/Source/Console/HPSDR/NetworkIO.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/

#include "RadioModel.h"
#include "RxDspWorker.h"
#include "core/RadioConnection.h"
#include "core/RadioConnectionTeardown.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/BoardCapabilities.h"
#include "core/HardwareProfile.h"
#include "core/ReceiverManager.h"
#include "core/AudioEngine.h"
#include "core/WdspEngine.h"
#include "core/RxChannel.h"
#include "core/AppSettings.h"
#include "core/SampleRateCatalog.h"
#include "core/LogCategories.h"
#include "core/NoiseFloorTracker.h"
#include "gui/SpectrumWidget.h"

#include <algorithm>
#include <cmath>

#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

RadioModel::RadioModel(QObject* parent)
    : QObject(parent)
    , m_discovery(new RadioDiscovery(this))
    , m_receiverManager(new ReceiverManager(this))
    , m_audioEngine(new AudioEngine(this))
    , m_wdspEngine(new WdspEngine(this))
{
    // Phase 3O: give AudioEngine a non-owning back-pointer to this model
    // so rxBlockReady() can look up per-slice mute / VAX state. Wired
    // immediately after construction; AudioEngine caches the pointer and
    // treats a null as a safe no-op (tests that build AudioEngine
    // standalone).
    m_audioEngine->setRadioModel(this);

    // Connection starts null — created by connectToRadio() via factory.
    //
    // Phase 3G-9b: the smooth-defaults profile is reachable only via the
    // "Reset to Smooth Defaults" button on SpectrumDefaultsPage per user
    // decision 2026-04-15 (Default should stay the out-of-box default).
    // No first-launch auto-apply here. The `DisplayProfileApplied`
    // AppSettings key is reserved for PR3 (Clarity) to repurpose.
}

RadioModel::~RadioModel()
{
    teardownConnection();
    qDeleteAll(m_slices);
    qDeleteAll(m_panadapters);
}

bool RadioModel::isConnected() const
{
    return m_connection && m_connection->isConnected();
}

const BoardCapabilities& RadioModel::boardCapabilities() const
{
    if (m_hardwareProfile.caps) { return *m_hardwareProfile.caps; }
    return BoardCapsTable::forBoard(HPSDRHW::Unknown);
}

// --- Slice Management ---

SliceModel* RadioModel::sliceAt(int index) const
{
    if (index >= 0 && index < m_slices.size()) {
        return m_slices.at(index);
    }
    return nullptr;
}

int RadioModel::addSlice()
{
    auto* slice = new SliceModel(this);
    int index = m_slices.size();
    slice->setSliceIndex(index);
    m_slices.append(slice);

    if (!m_activeSlice) {
        m_activeSlice = slice;
        emit activeSliceChanged(0);
    }

    emit sliceAdded(index);
    return index;
}

void RadioModel::removeSlice(int index)
{
    if (index < 0 || index >= m_slices.size()) {
        return;
    }

    SliceModel* slice = m_slices.takeAt(index);
    if (m_activeSlice == slice) {
        m_activeSlice = m_slices.isEmpty() ? nullptr : m_slices.first();
        emit activeSliceChanged(m_activeSlice ? 0 : -1);
    }

    delete slice;
    emit sliceRemoved(index);
}

void RadioModel::setActiveSlice(int index)
{
    if (index >= 0 && index < m_slices.size()) {
        m_activeSlice = m_slices.at(index);
        emit activeSliceChanged(index);
    }
}

// --- Panadapter Management ---

int RadioModel::addPanadapter()
{
    auto* pan = new PanadapterModel(this);
    int index = m_panadapters.size();
    m_panadapters.append(pan);

    // PanadapterModel::bandChanged fires when the pan center crosses a band
    // boundary. In NereusSDR's design m_lastBand tracks the VFO, not the pan
    // (see comment on the frequencyChanged lambda in wireSliceSignals), so
    // there is nothing to do here on a pan-centered crossing — per-band saves
    // flow from the VFO path and the coalesced scheduleSettingsSave() timer.
    // Intentionally left as a no-op hook so the connection survives future
    // per-pan band-aware behavior without re-adding the recursion/corruption
    // path that existed in v0.2.0.
    connect(pan, &PanadapterModel::bandChanged, this, [](Band /*newBand*/) {});

    emit panadapterAdded(index);
    return index;
}

void RadioModel::removePanadapter(int index)
{
    if (index < 0 || index >= m_panadapters.size()) {
        return;
    }

    delete m_panadapters.takeAt(index);
    emit panadapterRemoved(index);
}

// --- Connection ---

void RadioModel::connectToRadio(const RadioInfo& info)
{
    // Tear down any existing connection
    if (m_connection) {
        teardownConnection();
    }

    m_lastRadioInfo = info;
    m_intentionalDisconnect = false;

    // Compute HardwareProfile from model override (Phase 3I-RP).
    HPSDRModel selectedModel = info.modelOverride;
    if (selectedModel == HPSDRModel::FIRST) {
        selectedModel = defaultModelForBoard(info.boardType);
    }
    m_hardwareProfile = profileForModel(selectedModel);

    qCDebug(lcConnection) << "HardwareProfile: model=" << displayName(m_hardwareProfile.model)
                          << "effectiveBoard=" << static_cast<int>(m_hardwareProfile.effectiveBoard)
                          << "adcCount=" << m_hardwareProfile.adcCount;

    // Load per-MAC OC matrix state so the codec layer (P1/P2 buildCodecContext)
    // reads the correct per-band OC byte from the first C&C frame onwards.
    // Phase 3P-D Task 3.
    if (!info.macAddress.isEmpty()) {
        m_ocMatrix.setMacAddress(info.macAddress);
        m_ocMatrix.load();

        // Load per-MAC Alex antenna controller state so Antenna Control UI
        // and future protocol codecs read the correct per-band antenna assignments.
        // Phase 3P-F Task 3. Pattern mirrors OcMatrix above.
        m_alexController.setMacAddress(info.macAddress);
        m_alexController.load();

        // Load per-MAC Apollo accessory state (present/filter/tuner bools).
        // Phase 3P-F Task 5a.
        m_apolloController.setMacAddress(info.macAddress);
        m_apolloController.load();

        // Load per-MAC PennyLane ext-ctrl master toggle.
        // Phase 3P-F Task 5b.
        m_pennyLaneController.setMacAddress(info.macAddress);
        m_pennyLaneController.load();

        // Load per-MAC calibration state (freq correction factor, level offsets, etc.).
        // Phase 3P-G. Pushed to P2RadioConnection via setCalibrationController() below.
        m_calController.setMacAddress(info.macAddress);
        m_calController.load();
    }

    m_name = info.displayName();
    m_model = QString::fromLatin1(m_hardwareProfile.caps->displayName);
    m_version = QString::number(info.firmwareVersion);
    emit infoChanged();

    // Configure ReceiverManager with hardware capabilities
    m_receiverManager->setMaxReceivers(info.maxReceivers);

    // Create receiver 0 with protocol-appropriate DDC mapping.
    // P2 2-ADC boards (ANAN-G2/Saturn) use DDC2 as primary RX per
    // Thetis console.cs:8216 UpdateDDCs. P1 radios deliver samples on
    // hardware receiver index 0, so leave the mapping auto-assigned
    // (which rebuildHardwareMapping resolves to 0 for the first active
    // receiver). Hardcoding DDC2 for everything dropped every P1 ep6
    // packet at ReceiverManager::feedIqData on tester hardware.
    int rxIdx = m_receiverManager->createReceiver();
    if (info.protocol == ProtocolVersion::Protocol2) {
        m_receiverManager->setDdcMapping(rxIdx, 2);   // DDC2 for 2-ADC P2 boards
    }
    m_receiverManager->setAdcForReceiver(rxIdx, 0); // ADC0

    // Create slice 0 and load persisted VFO state from AppSettings
    if (m_slices.isEmpty()) {
        addSlice();
    }
    setActiveSlice(0);
    m_activeSlice->setReceiverIndex(rxIdx);
    loadSliceState(m_activeSlice);

    // Activate receiver (this sends hardwareReceiverCountChanged to RadioConnection)
    m_receiverManager->activateReceiver(rxIdx);

    // Initialize WDSP DSP engine (wisdom runs async — channel creation
    // is deferred until initializedChanged fires)
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // Sample rate + active RX count come from Hardware Config (per-MAC).
    // Falls back to Thetis default (192000, setup.cs:866) when nothing
    // is persisted, and to the board-cap first-entry if 192000 isn't
    // in the allowed list. wdspInSize follows the Thetis formula
    // 64 * rate / 48000 from ChannelMaster/cmsetup.c:104-111.
    const auto& caps = *m_hardwareProfile.caps;
    const HPSDRModel model = m_hardwareProfile.model;
    const int wdspInputRate = resolveSampleRate(
        AppSettings::instance(), info.macAddress, info.protocol, caps, model);
    const int wdspInSize = bufferSizeForRate(wdspInputRate);
    const int activeRxCount = resolveActiveRxCount(
        AppSettings::instance(), info.macAddress, caps);
    qCInfo(lcConnection) << "Connecting with sampleRate=" << wdspInputRate
                         << "inSize=" << wdspInSize
                         << "activeRxCount=" << activeRxCount;

    connect(m_wdspEngine, &WdspEngine::initializedChanged, this,
            [this, wdspInputRate, wdspInSize](bool ok) {
        if (!ok) {
            return;
        }
        // Create primary RX channel once WDSP is ready. in_size follows
        // Thetis cmaster.c create_rcvr: 64 * input_rate / 48000. WDSP
        // decimates input_rate -> 48000 internally and outputs 64 samples
        // per fexchange2 call.
        RxChannel* rxCh = m_wdspEngine->createRxChannel(0, wdspInSize, 4096,
                                                         wdspInputRate, 48000, 48000);
        if (rxCh) {
            // Apply slice state to WDSP channel (no longer hardcoded)
            if (m_activeSlice) {
                rxCh->setMode(m_activeSlice->dspMode());
                rxCh->setFilterFreqs(m_activeSlice->filterLow(),
                                     m_activeSlice->filterHigh());
                rxCh->setAgcMode(m_activeSlice->agcMode());
                rxCh->setAgcTop(m_activeSlice->rfGain());
                // AGC advanced — push slice state to WDSP (Stage 2)
                rxCh->setAgcThreshold(m_activeSlice->agcThreshold());
                rxCh->setAgcHang(m_activeSlice->agcHang());
                rxCh->setAgcSlope(m_activeSlice->agcSlope());
                rxCh->setAgcAttack(m_activeSlice->agcAttack());
                rxCh->setAgcDecay(m_activeSlice->agcDecay());
                rxCh->setAgcHangThreshold(m_activeSlice->agcHangThreshold());
                rxCh->setAgcFixedGain(m_activeSlice->agcFixedGain());
                rxCh->setAgcMaxGain(m_activeSlice->agcMaxGain());
                // NB2 sub-parameter defaults — From Thetis cmaster.c:55-68 (create_nobEXT)
                // These are set-and-forget; the run flag is gated by processIq() atomics.
                // Declared in specHPSDR.cs:922-937; WDSP nobII.c:658,686,697,707
                rxCh->setNb2Mode(0);         // cmaster.c:61  mode=0 (zero)
                rxCh->setNb2Tau(0.0001);     // cmaster.c:62  slewtime=0.0001 s
                rxCh->setNb2LeadTime(0.0001);// cmaster.c:63  advtime=0.0001 s
                rxCh->setNb2HangTime(0.0001);// cmaster.c:65  hangtime=0.0001 s
                // EMNR sub-parameter defaults — From Thetis radio.cs:2062,2081,2101,2235
                // These are set-and-forget on channel creation; run flag follows slice.
                rxCh->setEmnrGainMethod(2);   // radio.cs:2062 rx_nr2_gain_method = 2
                rxCh->setEmnrNpeMethod(0);    // radio.cs:2081 rx_nr2_npe_method = 0
                rxCh->setEmnrAeRun(true);     // radio.cs:2101 rx_nr2_ae_run = 1
                rxCh->setEmnrPosition(1);     // radio.cs:2235 rx_nr2_position = 1 (post-AGC)
                rxCh->setEmnrEnabled(m_activeSlice->emnrEnabled());
                rxCh->setSnbEnabled(m_activeSlice->snbEnabled());
                // APF sub-parameter defaults — From Thetis radio.cs:1986,1948,1967,1929
                // These are set-and-forget on channel creation; run flag follows slice.
                // selection=3 (bi-quad), bw=600Hz, gain=1.0, freq=600.0Hz
                rxCh->setApfSelection(3);       // radio.cs:1986 _rx_apf_type = 3 (bi-quad)
                rxCh->setApfBandwidth(600.0);   // radio.cs:1948 rx_apf_bw = 600.0 Hz
                rxCh->setApfGain(1.0);          // radio.cs:1967 rx_apf_gain = 1.0
                rxCh->setApfFreq(600.0);        // radio.cs:1929 rx_apf_freq = 600.0 Hz
                rxCh->setApfEnabled(m_activeSlice->apfEnabled());
                // Squelch initial push — From Thetis radio.cs:1185,1164,1274,1293,1312
                rxCh->setSsqlEnabled(m_activeSlice->ssqlEnabled());
                // Model stores 0–100 (slider units); WDSP expects 0.0–1.0 linear.
                rxCh->setSsqlThresh(std::clamp(m_activeSlice->ssqlThresh() / 100.0, 0.0, 1.0));
                rxCh->setAmsqEnabled(m_activeSlice->amsqEnabled());
                rxCh->setAmsqThresh(m_activeSlice->amsqThresh());
                rxCh->setFmsqEnabled(m_activeSlice->fmsqEnabled());
                rxCh->setFmsqThresh(m_activeSlice->fmsqThresh());
                // Audio panel initial push
                // Mute: From Thetis dsp.cs:393-394 — panel runs by default (unmuted)
                // Pan: From Thetis radio.cs:1386 — pan = 0.5f (center); NereusSDR 0.0 center
                // Binaural: From Thetis radio.cs:1145 — bin_on = false (dual-mono)
                rxCh->setMuted(m_activeSlice->muted());
                rxCh->setAudioPan(m_activeSlice->audioPan());
                rxCh->setBinauralEnabled(m_activeSlice->binauralEnabled());
            }
            rxCh->setActive(true);
        }
        // Apply volume from slice
        if (m_activeSlice) {
            m_audioEngine->setVolume(m_activeSlice->afGain() / 100.0f);
        }
        // Start audio output
        m_audioEngine->start();
        qCInfo(lcDsp) << "WDSP ready — RX channel 0 active, audio started";
    }, Qt::SingleShotConnection);
    m_wdspEngine->initialize(configDir);

    // Factory-create the connection (no parent — will be moved to thread)
    auto conn = RadioConnection::create(info);
    if (!conn) {
        qCWarning(lcConnection) << "Failed to create connection for" << info.displayName();
        return;
    }
    m_connection = conn.release();
    m_connection->setHardwareProfile(m_hardwareProfile);

    // Wire the OcMatrix so P1/P2 buildCodecContext() can source ctx.ocByte
    // from maskFor(currentBand, mox) at C&C compose time.  Must be called
    // before the connection thread starts.  Phase 3P-D Task 3.
    if (auto* p1 = qobject_cast<class P1RadioConnection*>(m_connection)) {
        p1->setOcMatrix(&m_ocMatrix);
    } else if (auto* p2 = qobject_cast<class P2RadioConnection*>(m_connection)) {
        p2->setOcMatrix(&m_ocMatrix);
    }

    // Wire CalibrationController to P2RadioConnection so hzToPhaseWord()
    // applies effectiveFreqCorrectionFactor(). P1 uses raw Hz (not phase words),
    // so P1 doesn't need this. Phase 3P-G.
    if (auto* p2 = qobject_cast<class P2RadioConnection*>(m_connection)) {
        p2->setCalibrationController(&m_calController);
    }

    // Wire IoBoardHl2 so P1CodecHl2 can dequeue I2C transactions into C&C
    // frames and the ep6 read path can route responses back to the register
    // mirror.  On non-HL2 boards, setIoBoard() is a noop (selectCodec()
    // won't have installed a P1CodecHl2).  Phase 3P-E Task 2.
    if (auto* p1 = qobject_cast<class P1RadioConnection*>(m_connection)) {
        p1->setIoBoard(&m_ioBoard);
    }

    // Wire HermesLiteBandwidthMonitor so P1RadioConnection can record ep6/ep2
    // byte counts and drive the throttle-detection tick from onWatchdogTick().
    // The monitor is owned by RadioModel; the connection holds a non-owning ptr.
    // Phase 3P-E Task 3.
    if (auto* p1 = qobject_cast<class P1RadioConnection*>(m_connection)) {
        m_bwMonitor.reset();
        p1->setBandwidthMonitor(&m_bwMonitor);
    }

    // Create worker thread
    m_connThread = new QThread(this);
    m_connThread->setObjectName(QStringLiteral("ConnectionThread"));

    // Move connection to worker thread BEFORE wiring signals
    m_connection->moveToThread(m_connThread);

    // Wire signals (auto-queued across threads). Pass wdspInSize so the
    // DSP worker's accumulator drains in chunks that match the in_size
    // we just opened the WDSP channel with.
    wireConnectionSignals(wdspInSize);

    // Start thread — init() will be called on the worker thread
    connect(m_connThread, &QThread::started, m_connection, &RadioConnection::init);
    m_connThread->start();

    // CRITICAL: push sample rate + VFO frequency to the connection BEFORE
    // dispatching connectToRadio. The worker thread dequeues invokeMethod
    // calls in FIFO order, so whatever we queue first runs first. If we
    // queue connectToRadio before the setters, connectToRadio -> sendCommandFrame
    // -> composeEp2Frame reads m_rxFreqHz[0]=0 and m_sampleRate=48000 defaults
    // and sends a primed ep2 frame with phase word 0 to the radio just before
    // metis-start. Result: radio initializes DDC at freq=0 (bypass/idle state)
    // and streams ADC-pinned data with Q=0 forever. Verified against Thetis
    // NetworkIO.cs flow: Thetis always sets SetDDCRate + SetVFOfreq BEFORE
    // SendStartToMetis, so ForceCandCFrame inside SendStartToMetis reads the
    // correct freq/rate from globals.
    const int wireSampleRate = wdspInputRate;
    QMetaObject::invokeMethod(m_connection, [conn = m_connection, wireSampleRate]() {
        conn->setSampleRate(wireSampleRate);
    });
    // Push active receiver count to the connection. P1 uses this to encode
    // nrx bits in the C&C bank 0 frame. P2 DDC assignment is more complex
    // (Thetis console.cs:8216 UpdateDDCs — DDC2 is primary, not DDC0) and
    // is handled inside P2RadioConnection::connectToRadio. Calling
    // setActiveReceiverCount on P2 here would enable DDC0..N-1 on top of
    // the DDC2 enable that connectToRadio sets, leaving extra DDCs active.
    // Deferred to Phase 3F (multi-panadapter) which ports UpdateDDCs().
    if (info.protocol == ProtocolVersion::Protocol1) {
        QMetaObject::invokeMethod(m_connection, [conn = m_connection, activeRxCount]() {
            conn->setActiveReceiverCount(activeRxCount);
        });
    }
    if (m_activeSlice) {
        int hwRx = m_receiverManager->receiverConfig(0).hardwareRx;
        if (hwRx < 0) { hwRx = 0; }
        quint64 freqHz = m_activeSlice->frequency();
        QMetaObject::invokeMethod(m_connection, [conn = m_connection, hwRx, freqHz]() {
            conn->setReceiverFrequency(hwRx, freqHz);
        });
    }

    // Now dispatch connectToRadio -- it will find the correct m_rxFreqHz[0]
    // and m_sampleRate when sendCommandFrame runs inside it.
    QMetaObject::invokeMethod(m_connection, [conn = m_connection, info]() {
        conn->connectToRadio(info);
    });

    // Tell MainWindow / FFTEngine / SpectrumWidget the wire rate so bin math
    // matches the persisted hardware rate. Without this the FFT uses a stale
    // rate and compresses/expands the spectrum incorrectly.
    emit wireSampleRateChanged(static_cast<double>(wireSampleRate));

    qCDebug(lcConnection) << "Connecting to" << info.displayName()
                          << "P" << static_cast<int>(info.protocol);
}

void RadioModel::disconnectFromRadio()
{
    m_intentionalDisconnect = true;
    teardownConnection();
}

void RadioModel::wireConnectionSignals(int wdspInSize)
{
    if (!m_connection) {
        return;
    }

    // Connection state → RadioModel (auto-queued: connection thread → main thread)
    connect(m_connection, &RadioConnection::connectionStateChanged,
            this, &RadioModel::onConnectionStateChanged);

    // --- Slice → WDSP + RadioConnection ---
    // Wire active slice property changes to WDSP DSP engine and radio hardware.
    wireSliceSignals();

    // --- I/Q data → ReceiverManager → DSP worker → WDSP → AudioEngine ---
    // Route through ReceiverManager for DDC-aware mapping, then dispatch
    // to RxDspWorker on its own thread for fexchange2 processing.

    // Step 1: RadioConnection I/Q → ReceiverManager (DDC routing).
    // Auto connection: m_connection is on its worker thread, this is on
    // main, so the slot is queued onto the main thread.
    connect(m_connection, &RadioConnection::iqDataReceived,
            this, [this](int ddcIndex, const QVector<float>& samples) {
        m_receiverManager->feedIqData(ddcIndex, samples);
    });

    // Step 2a: ReceiverManager → spectrum fork (main thread, fast).
    // Kept on the main thread so rawIqData → FFTEngine routing stays
    // unchanged. FFTEngine lives on its own SpectrumThread and the
    // signal already crosses threads via queued connection.
    connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
            this, [this](int receiverIndex, const QVector<float>& samples) {
        Q_UNUSED(receiverIndex);
        emit rawIqData(samples);
    });

    // Step 2b: ReceiverManager → DSP worker (queued, off the main thread).
    // RxDspWorker accumulates samples into in_size chunks, runs each
    // chunk through RxChannel::processIq → fexchange2, then forwards
    // decoded audio to AudioEngine. fexchange2 must NOT run on the
    // main/GUI thread — see RxDspWorker.h for the deadlock rationale.
    Q_ASSERT(m_dspThread == nullptr && m_dspWorker == nullptr);
    m_dspThread = new QThread(this);
    m_dspThread->setObjectName(QStringLiteral("DspThread"));
    m_dspWorker = new RxDspWorker();   // no parent — moved to thread
    m_dspWorker->setEngines(m_wdspEngine, m_audioEngine);
    // Per-rate accumulator drain size. Must match the in_size that
    // WdspEngine::createRxChannel was called with above (line ~452),
    // otherwise fexchange2 sees the wrong sample count per call and
    // produces glitchy / jittery audio. WDSP RX output is always 64
    // samples per call (input_rate → 48000 decimation, dual-mono
    // panel via SetRXAPanelBinaural).
    m_dspWorker->setBufferSizes(wdspInSize, 64);
    m_dspWorker->moveToThread(m_dspThread);
    connect(m_dspThread, &QThread::finished,
            m_dspWorker, &QObject::deleteLater);
    connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
            m_dspWorker, &RxDspWorker::processIqBatch,
            Qt::QueuedConnection);
    m_dspThread->start();

    // Meter data → MeterModel
    connect(m_connection, &RadioConnection::meterDataReceived,
            this, [this](float fwd, float rev, float voltage, float current) {
        Q_UNUSED(voltage);
        Q_UNUSED(current);
        Q_UNUSED(fwd);
        Q_UNUSED(rev);
    });

    // Error handling
    connect(m_connection, &RadioConnection::errorOccurred,
            this, [this](NereusSDR::RadioConnectionError code, const QString& msg) {
        Q_UNUSED(code);
        qCWarning(lcConnection) << "Connection error:" << msg;
    });

    // ReceiverManager → RadioConnection (hardware updates)
    connect(m_receiverManager, &ReceiverManager::hardwareReceiverCountChanged,
            this, [this](int count) {
        if (m_connection) {
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, count]() {
                conn->setActiveReceiverCount(count);
            });
        }
    });

    connect(m_receiverManager, &ReceiverManager::hardwareFrequencyChanged,
            this, [this](int hwIndex, quint64 freq) {
        if (m_connection) {
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, hwIndex, freq]() {
                conn->setReceiverFrequency(hwIndex, freq);
            });
        }
    });
}

// Wire active slice signals to WDSP channel and radio hardware.
// Called from wireConnectionSignals after connection is established.
void RadioModel::wireSliceSignals()
{
    if (!m_activeSlice || !m_connection) {
        return;
    }

    SliceModel* slice = m_activeSlice;

    // Frequency → ReceiverManager → radio hardware
    // ReceiverManager handles DDC mapping (receiver 0 → DDC2 for ANAN-G2)
    connect(slice, &SliceModel::frequencyChanged, this, [this, slice](double freq) {
        int rxIdx = slice->receiverIndex();
        if (rxIdx >= 0) {
            m_receiverManager->setReceiverFrequency(rxIdx, static_cast<quint64>(freq));
        }
        // TX follows RX (simplex)
        if (m_connection) {
            quint64 freqHz = static_cast<quint64>(freq);
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, freqHz]() {
                conn->setTxFrequency(freqHz);
            });
        }
        // Track band from VFO frequency so per-band saves target the correct
        // band even when the panadapter center hasn't crossed the boundary.
        //
        // Do NOT recall bandstack state on a VFO-driven band crossing. From
        // Thetis console.cs:45312 handleBSFChange [v2.10.3.13 @501e3f5]:
        // on an oldBand != newBand transition, Thetis only updates the old
        // and new band's LastVisited records — it does not restore saved
        // DSP state. Bandstack recall is reserved for the explicit
        // band-button press path. Trying to recall here on every wheel-tune
        // caused two bugs in v0.2.0: (1) the VFO snaps to the newBand's
        // stored frequency, breaking smooth wheel-tune across boundaries;
        // (2) saveToSettings(oldBand) wrote the current (now post-tune)
        // frequency into the oldBand slot — corrupting the stored value
        // for that band. Letting the coalesced scheduleSettingsSave() flush
        // keeps the CURRENT band's slot up to date without either bug.
        Band newBand = bandFromFrequency(freq);
        if (newBand != m_lastBand) {
            m_lastBand = newBand;
        }
        scheduleSettingsSave();
    });

    // Mode → WDSP
    connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setMode(mode);
        }
        scheduleSettingsSave();
    });

    // Filter → WDSP
    connect(slice, &SliceModel::filterChanged, this, [this](int low, int high) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setFilterFreqs(low, high);
        }
        scheduleSettingsSave();
    });

    // AGC → WDSP
    connect(slice, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcMode(mode);
        }
        scheduleSettingsSave();
    });

    // AGC advanced → WDSP
    // From Thetis Project Files/Source/Console/console.cs:45977 — AGCThresh
    // From Thetis Project Files/Source/Console/radio.cs:1037-1124 — Decay/Hang/Slope
    // From Thetis Project Files/Source/Console/dsp.cs:116-120 — P/Invoke decls
    //
    // Bidirectional sync: SetRXAAGCThresh and SetRXAAGCTop both write max_gain
    // in WDSP wcpAGC.c. After either changes, read back the sibling value and
    // update the paired control. m_syncingAgc guards against A→B→A feedback loops.
    // From Thetis console.cs:45960-46006 — bidirectional AGC sync pattern.
    connect(slice, &SliceModel::agcThresholdChanged, this, [this](int dBu) {
        if (m_syncingAgc) { return; }

        // From Thetis v2.10.3.13 console.cs:49129-49130 — manual drag disables auto
        SliceModel* s = m_activeSlice;
        if (s && s->autoAgcEnabled()) {
            s->setAutoAgcEnabled(false);
        }

        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            m_syncingAgc = true;
            rxCh->setAgcThreshold(dBu);
            // Read back resulting AGC Top and sync RF Gain display.
            // From Thetis console.cs:45978 — GetRXAAGCTop after SetRXAAGCThresh
            double top = rxCh->readBackAgcTop();
            int rfGain = static_cast<int>(std::round(top));
            SliceModel* s = m_activeSlice;
            if (s && s->rfGain() != rfGain) {
                s->setRfGain(rfGain);
            }
            m_syncingAgc = false;
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcHangChanged, this, [this](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcHang(ms);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcSlopeChanged, this, [this](int slope) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcSlope(slope);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcAttackChanged, this, [this](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcAttack(ms);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcDecayChanged, this, [this](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcDecay(ms);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9081 — hang threshold
    connect(slice, &SliceModel::agcHangThresholdChanged, this, [this](int val) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcHangThreshold(val);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9001 — fixed gain
    connect(slice, &SliceModel::agcFixedGainChanged, this, [this](int dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcFixedGain(dB);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9011 — max gain
    connect(slice, &SliceModel::agcMaxGainChanged, this, [this](int dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcMaxGain(dB);
        }
        scheduleSettingsSave();
    });

    // ── Auto AGC-T timer ────────────────────────────────────────────────
    // From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC_Tick, 500ms interval
    m_autoAgcTimer = new QTimer(this);
    m_autoAgcTimer->setInterval(500);
    connect(m_autoAgcTimer, &QTimer::timeout, this, [this]() {
        SliceModel* slice = m_activeSlice;
        if (!slice || !slice->autoAgcEnabled()) {
            return;
        }
        // From Thetis v2.10.3.13 console.cs:46059 — guard: skip if not connected or MOX
        if (!m_connection || !m_connection->isConnected()) {
            return;
        }
        // From Thetis v2.10.3.13 console.cs:46059 — if (!chkPower.Checked || _mox) return;
        if (m_transmitModel.isMox()) {
            return;
        }
        if (!m_noiseFloorTracker || !m_noiseFloorTracker->isGood()) {
            return;
        }

        // From Thetis v2.10.3.13 console.cs:46107-46115
        const double noiseFloor = static_cast<double>(m_noiseFloorTracker->noiseFloor());

        // From Thetis v2.10.3.13 console.cs:33292-33319 — agcCalOffset(rx)
        // Simplified: 0.0f for FIXD (NereusSDR AGCMode::Off), 2.0f for others
        // Full formula: 2.0f + (DisplayCalOffset + PreampOffset - AlexPreampOffset - FFTSizeOffset)
        // Alex/preamp/FFT-size offsets land with spectrum knee line overlay
        const float calOffset = (slice->agcMode() == AGCMode::Off)
            ? 0.0f : 2.0f;

        // From Thetis v2.10.3.13 console.cs:45965-45968 — apply cal offset
        const double threshold = (noiseFloor + slice->autoAgcOffset())
                                 - static_cast<double>(calOffset);

        // From Thetis v2.10.3.13 console.cs:45969-45970 — clamp [-160, +2]
        const double clamped = std::clamp(threshold, -160.0, 2.0);
        const int threshInt = static_cast<int>(std::round(clamped));

        // Update both WDSP and model. m_syncingAgc prevents the
        // agcThresholdChanged handler from disabling auto mode AND from
        // re-entering the WDSP call, so we must call RxChannel directly.
        if (slice->agcThreshold() != threshInt) {
            m_syncingAgc = true;

            // Direct WDSP update — the signal handler is blocked by m_syncingAgc
            RxChannel* rxCh = m_wdspEngine ? m_wdspEngine->rxChannel(0) : nullptr;
            if (rxCh) {
                rxCh->setAgcThreshold(threshInt);
                // From Thetis v2.10.3.13 console.cs:45978 — readback AGC top
                double top = rxCh->readBackAgcTop();
                int rfGain = static_cast<int>(std::round(top));
                if (slice->rfGain() != rfGain) {
                    slice->setRfGain(rfGain);
                }
            }

            // Update model (UI sync) — handler won't re-enter WDSP
            slice->setAgcThreshold(threshInt);
            m_syncingAgc = false;
        }
    });
    m_autoAgcTimer->start();

    // EMNR (NR2) → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:2216-2232
    // WDSP: third_party/wdsp/src/emnr.c:1283
    connect(slice, &SliceModel::emnrEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setEmnrEnabled(on);
        }
        scheduleSettingsSave();
    });

    // SNB → WDSP
    // From Thetis Project Files/Source/Console/console.cs:36347
    //   WDSP.SetRXASNBARun(WDSP.id(0, 0), chkDSPNB2.Checked)
    // WDSP: third_party/wdsp/src/snb.c:579
    connect(slice, &SliceModel::snbEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setSnbEnabled(on);
        }
        scheduleSettingsSave();
    });

    // APF → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1910-1927
    //   WDSP.SetRXASPCWRun(WDSP.id(thread, subrx), value)
    // WDSP: third_party/wdsp/src/apfshadow.c:93
    connect(slice, &SliceModel::apfEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setApfEnabled(on);
        }
        scheduleSettingsSave();
    });

    // APF tune offset → WDSP freq
    // From Thetis Project Files/Source/Console/setup.cs:17068-17073
    //   freq = CWPitch + tuneOffset; slider offset range -250..+250
    //   CW pitch default 600 Hz from Thetis console.cs
    // WDSP: third_party/wdsp/src/apfshadow.c:117
    connect(slice, &SliceModel::apfTuneHzChanged, this, [this](int hz) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            // From Thetis setup.cs:17071 — freq = CWPitch + tuneOffset
            // CW pitch default 600 Hz from Thetis console.cs
            static constexpr double kCwPitchHz = 600.0;
            rxCh->setApfFreq(kCwPitchHz + static_cast<double>(hz));
        }
        scheduleSettingsSave();
    });

    // Squelch — SSB → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1185-1229
    // WDSP: third_party/wdsp/src/ssql.c:331,339
    connect(slice, &SliceModel::ssqlEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setSsqlEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::ssqlThreshChanged, this, [this](double threshold) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            // Model stores 0–100 (slider units); WDSP expects 0.0–1.0 linear.
            // From Thetis radio.cs:1217-1218 — clamped 0..1, default 0.16.
            double normalized = std::clamp(threshold / 100.0, 0.0, 1.0);
            rxCh->setSsqlThresh(normalized);
        }
        scheduleSettingsSave();
    });

    // Squelch — AM → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1164-1178, 1293-1310
    // WDSP: third_party/wdsp/src/amsq.c (SetRXAAMSQRun, SetRXAAMSQThreshold)
    connect(slice, &SliceModel::amsqEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAmsqEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::amsqThreshChanged, this, [this](double dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAmsqThresh(dB);
        }
        scheduleSettingsSave();
    });

    // Squelch — FM → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1274-1329
    // WDSP: third_party/wdsp/src/fmsq.c:236,244
    // SliceModel stores fmsqThresh in dB; RxChannel::setFmsqThresh converts to linear
    connect(slice, &SliceModel::fmsqEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setFmsqEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::fmsqThreshChanged, this, [this](double dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setFmsqThresh(dB);
        }
        scheduleSettingsSave();
    });

    // Audio panel — mute / pan / binaural → WDSP PatchPanel
    // From Thetis Project Files/Source/Console/radio.cs:1386-1403 (pan)
    // From Thetis Project Files/Source/Console/radio.cs:1145-1162 (binaural)
    // WDSP: third_party/wdsp/src/patchpanel.c:126,159,187
    connect(slice, &SliceModel::mutedChanged, this, [this](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setMuted(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::audioPanChanged, this, [this](double pan) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAudioPan(pan);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::binauralEnabledChanged, this, [this](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setBinauralEnabled(v);
        }
        scheduleSettingsSave();
    });

    // RIT + DIG offset → WDSP shift frequency
    //
    // RIT (Receive Incremental Tuning): client-side demodulation offset that
    // does NOT retune the hardware VFO.
    // From Thetis console.cs — RIT adjusts receive demodulation without moving
    // the hardware DDC center.
    //
    // DIG offset: per-mode click-tune demodulation offset for DIGL/DIGU.
    // From Thetis console.cs:14637 (DIGUClickTuneOffset) and :14672
    // (DIGLClickTuneOffset). Both are int offsets in Hz; Thetis uses per-mode
    // filter re-centering internally, but NereusSDR implements DIG offset as
    // an additive shift on the same setShiftFrequency path as RIT.
    //
    // Combined: shift = ritOffset + digOffset (where digOffset is mode-gated).
    // For 3G-10 (single RX, no CTUN), the shift = these two terms only.
    auto updateShiftFrequency = [this, slice]() {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (!rxCh) { return; }
        double offset = slice->ritEnabled()
                        ? static_cast<double>(slice->ritHz())
                        : 0.0;
        // DIG offset per mode — Thetis console.cs:14637,14672
        if (slice->dspMode() == DSPMode::DIGL) {
            offset += static_cast<double>(slice->diglOffsetHz());
        } else if (slice->dspMode() == DSPMode::DIGU) {
            offset += static_cast<double>(slice->diguOffsetHz());
        }
        rxCh->setShiftFrequency(offset);
    };
    connect(slice, &SliceModel::ritEnabledChanged,  this, updateShiftFrequency);
    connect(slice, &SliceModel::ritHzChanged,        this, updateShiftFrequency);
    connect(slice, &SliceModel::diglOffsetHzChanged, this, updateShiftFrequency);
    connect(slice, &SliceModel::diguOffsetHzChanged, this, updateShiftFrequency);
    connect(slice, &SliceModel::dspModeChanged,      this, updateShiftFrequency);

    // RTTY mark + shift → bandpass filter
    //
    // RTTY uses two audio tones: mark (freq1 = 2295 Hz) and space (freq0 = 2125 Hz).
    // From Thetis radio.cs:2024-2060 — rx_dolly_freq0/freq1 are stored and fed to
    // SetRXAmpeakFilFreq (the IIR audio peak filter / "dolly" filter). NereusSDR
    // does not yet implement the ampeak dolly filter (it is not in RxChannel),
    // so the mark/shift values are used to compute a bandpass window that covers
    // both tones:
    //   filterLow  = markHz − shiftHz/2 − 100
    //   filterHigh = markHz + shiftHz/2 + 100
    // The ±100 Hz guard band keeps both tones well inside the passband.
    //
    // Note: Thetis uses DIGU/DIGL DSP modes for RTTY (there is no DSPMode::RTTY
    // in WDSP — see Thetis enums.cs:252-268). The bandpass update fires whenever
    // mark or shift changes, matching Thetis setup.cs:17203 (udDSPRX1DollyF0_ValueChanged)
    // which fires unconditionally on control change.
    //
    // Full dolly-filter support (SetRXAmpeakFilFreq wiring) is deferred to a later
    // phase when the ampeak API is added to RxChannel.
    auto updateRttyFilter = [this, slice]() {
        const int mark  = slice->rttyMarkHz();
        const int shift = slice->rttyShiftHz();
        const int low   = mark - shift / 2 - 100;
        const int high  = mark + shift / 2 + 100;
        slice->setFilter(low, high);
    };
    connect(slice, &SliceModel::rttyMarkHzChanged,  this, updateRttyFilter);
    connect(slice, &SliceModel::rttyShiftHzChanged, this, updateRttyFilter);

    // XIT stored for 3M-1 (TX phase) to consume on keydown. No RX effect in 3G-10.

    // AF gain → AudioEngine volume
    connect(slice, &SliceModel::afGainChanged, this, [this](int gain) {
        m_audioEngine->setVolume(gain / 100.0f);
        scheduleSettingsSave();
    });

    // RF gain → WDSP AGC top, with bidirectional sync back to AGC-T.
    // From Thetis console.cs:50350 pattern — GetRXAAGCThresh after SetRXAAGCTop
    connect(slice, &SliceModel::rfGainChanged, this, [this](int gain) {
        if (m_syncingAgc) { return; }
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            m_syncingAgc = true;
            rxCh->setAgcTop(static_cast<double>(gain));
            // Read back resulting threshold and sync AGC-T display.
            double thresh = rxCh->readBackAgcThresh();
            int threshInt = static_cast<int>(std::round(thresh));
            SliceModel* s = m_activeSlice;
            if (s && s->agcThreshold() != threshInt) {
                s->setAgcThreshold(threshInt);
            }
            m_syncingAgc = false;
        }
        scheduleSettingsSave();
    });

    // Antenna changes → Alex register via RadioConnection
    connect(slice, &SliceModel::rxAntennaChanged, this, [this](const QString& ant) {
        if (m_connection) {
            // Map antenna name to index: ANT1=0, ANT2=1, ANT3=2
            int idx = 0;
            if (ant == QLatin1String("ANT2")) { idx = 1; }
            else if (ant == QLatin1String("ANT3")) { idx = 2; }
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, idx]() {
                conn->setAntenna(idx);
            });
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::txAntennaChanged, this, [this](const QString&) {
        // TX antenna is set in the same setAntenna call for now
        // Full TX-specific antenna routing deferred to TX implementation
        scheduleSettingsSave();
    });

    // Send initial frequency to radio (after connection init completes)
    QTimer::singleShot(100, this, [this, slice]() {
        if (m_connection && m_connection->isConnected()) {
            int rxIdx = slice->receiverIndex();
            quint64 freqHz = static_cast<quint64>(slice->frequency());
            if (rxIdx >= 0) {
                m_receiverManager->setReceiverFrequency(rxIdx, freqHz);
            }
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, freqHz]() {
                conn->setTxFrequency(freqHz);
            });
        }
    });
}

// Load persisted VFO state from AppSettings into a slice.
// Migrates legacy flat keys first, then restores per-band state for the
// current band (derived from the panadapter center frequency or the slice
// default frequency).
void RadioModel::loadSliceState(SliceModel* slice)
{
    if (!slice) {
        return;
    }

    // One-shot migration of legacy Vfo* flat keys. No-op if already migrated.
    SliceModel::migrateLegacyKeys();

    // Derive current band. Use the first panadapter's band if available;
    // otherwise fall back to bandFromFrequency on the slice's default freq.
    Band currentBand = Band::Band20m;
    if (!m_panadapters.isEmpty()) {
        currentBand = m_panadapters.first()->band();
    } else {
        currentBand = bandFromFrequency(slice->frequency());
    }
    m_lastBand = currentBand;

    slice->restoreFromSettings(currentBand);

    qCInfo(lcDsp) << "Loaded slice state for band:"
                  << bandKeyName(currentBand)
                  << SliceModel::modeName(slice->dspMode())
                  << slice->frequency() / 1e6 << "MHz"
                  << "AGC:" << static_cast<int>(slice->agcMode())
                  << "AF:" << slice->afGain() << "RF:" << slice->rfGain();
}

// Coalesce settings saves to avoid writing on every scroll tick.
void RadioModel::scheduleSettingsSave()
{
    if (m_settingsSaveScheduled) {
        return;
    }
    m_settingsSaveScheduled = true;
    QTimer::singleShot(500, this, [this]() {
        m_settingsSaveScheduled = false;
        saveSliceState(m_activeSlice);
    });
}

// Persist current slice state to AppSettings (per-band + session state).
void RadioModel::saveSliceState(SliceModel* slice)
{
    if (!slice) {
        return;
    }

    slice->saveToSettings(m_lastBand);
}

void RadioModel::teardownConnection()
{
    if (!m_connection) {
        return;
    }

    // Disconnect signals into the DSP worker first so no new I/Q
    // batches can be posted onto the worker thread, then quit and
    // join that thread before touching WDSP. The worker is queued
    // for deletion via QThread::finished (see wireConnectionSignals),
    // so the m_dspWorker pointer may dangle after wait() returns —
    // null it out to avoid a use-after-free in any later teardown.
    if (m_dspWorker != nullptr) {
        QObject::disconnect(m_receiverManager, nullptr, m_dspWorker, nullptr);
    }
    if (m_dspThread != nullptr) {
        m_dspThread->quit();
        m_dspThread->wait();
        delete m_dspThread;
        m_dspThread = nullptr;
        m_dspWorker = nullptr;
    }

    // Stop audio output
    m_audioEngine->stop();

    // Shutdown WDSP (destroys all channels, saves cache)
    m_wdspEngine->shutdown();

    // Disconnect remaining signals (prevents new work being queued)
    QObject::disconnect(m_connection, nullptr, this, nullptr);
    QObject::disconnect(m_connection, nullptr, m_receiverManager, nullptr);
    QObject::disconnect(m_receiverManager, nullptr, this, nullptr);

    // Drop all logical receivers so the next connectToRadio() starts from
    // index 0 with a fresh wdspChannel counter. Without this, issue #75:
    // receiver 0 leaks into the next session, createReceiver() returns 1,
    // and on P2 2-ADC boards both receivers claim DDC2 — the collision in
    // rebuildHardwareMapping routes DDC2 I/Q to logical 1 whose wdspChannel
    // is 1, but only WDSP channel 0 is created in connectToRadio, so audio
    // and spectrum silently drop on the second connect.
    m_receiverManager->reset();

    // Tear down the connection on its own worker thread via the shared
    // helper. See src/core/RadioConnectionTeardown.h for why this must
    // run on the worker — short version: the RadioConnection's QTimers
    // are thread-affined to the worker and destroying them on any other
    // thread emits cross-thread warnings and can crash on Windows.
    teardownWorkerThreadedConnection(m_connection, m_connThread);
}

// Phase 3G-9b — 7 smooth-default recipe values. See docs/architecture/waterfall-tuning.md.
void RadioModel::applyClaritySmoothDefaults()
{
    SpectrumWidget* sw = spectrumWidget();
    if (!sw) { return; }  // not yet wired by MainWindow — Task 3 re-invokes

    // 1. Palette — narrow-band monochrome. See docs/architecture/waterfall-tuning.md §1.
    sw->setWfColorScheme(WfColorScheme::ClarityBlue);

    // 2. Spectrum averaging mode — log-recursive for heavy smoothing.
    sw->setAverageMode(AverageMode::Logarithmic);

    // 3. Averaging alpha — very slow exponential (~500 ms perceived smoothing
    //    at 30 FPS). See waterfall-tuning.md §3.
    sw->setAverageAlpha(0.05f);

    // 4. Trace colour — pure white, thin, sits cleanly in front of the
    //    waterfall without competing. Visual target: 2026-04-14 reference.
    sw->setFillColor(QColor(0xff, 0xff, 0xff, 230));

    // 5. Pan fill OFF — trace renders as a thin line, not a filled curve.
    //    NereusSDR's default is fill-on; turn it off to match the reference.
    sw->setPanFillEnabled(false);

    // 6. Waterfall AGC — tracks band conditions automatically. With AGC on,
    sw->setWfAgcEnabled(true);

    // 7. Waterfall update period — 30 ms for smooth scroll motion.
    sw->setWfUpdatePeriodMs(30);

    // Mark the profile as applied so the gate short-circuits on next launch.
    AppSettings::instance().setValue(
        QStringLiteral("DisplayProfileApplied"),
        QStringLiteral("True"));
}

void RadioModel::onConnectionStateChanged(ConnectionState state)
{
    emit connectionStateChanged();

    switch (state) {
    case ConnectionState::Connected:
        qCDebug(lcConnection) << "Connected to" << m_name;
        // Phase 3I Task 17 — record the most recently used radio so
        // tryAutoReconnect() targets the right entry on next launch.
        if (!m_lastRadioInfo.macAddress.isEmpty()) {
            AppSettings& s = AppSettings::instance();
            s.setLastConnected(m_lastRadioInfo.macAddress);
            s.save();
            // Exempt this MAC from discovery stale-removal — once the
            // radio is streaming it stops replying to broadcasts.
            m_discovery->setConnectedMac(m_lastRadioInfo.macAddress);
        }
        // Phase 3I — fan out to HardwarePage so its sub-tabs populate with
        // the connected radio's fields (Radio Info labels, sample rate,
        // capability-gated tab visibility, per-MAC settings restore).
        emit currentRadioChanged(m_lastRadioInfo);
        break;
    case ConnectionState::Disconnected:
        qCDebug(lcConnection) << "Disconnected from" << m_name;
        m_discovery->clearConnectedMac();
        break;
    case ConnectionState::Connecting:
        qCDebug(lcConnection) << "Connecting to" << m_name << "...";
        break;
    case ConnectionState::Error:
        qCWarning(lcConnection) << "Connection error for" << m_name;
        m_discovery->clearConnectedMac();
        break;
    }
}

} // namespace NereusSDR
