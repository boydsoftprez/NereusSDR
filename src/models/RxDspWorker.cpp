// =================================================================
// src/models/RxDspWorker.cpp  (NereusSDR)
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

#include "RxDspWorker.h"

#include "core/AudioEngine.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"

namespace NereusSDR {

RxDspWorker::RxDspWorker(QObject* parent)
    : QObject(parent)
{
    m_iqAccumI.reserve(kDefaultInSize * 2);
    m_iqAccumQ.reserve(kDefaultInSize * 2);
}

RxDspWorker::~RxDspWorker() = default;

void RxDspWorker::setEngines(WdspEngine* wdsp, AudioEngine* audio)
{
    m_wdspEngine  = wdsp;
    m_audioEngine = audio;
}

void RxDspWorker::setBufferSizes(int inSize, int outSize)
{
    m_inSize.store(inSize, std::memory_order_relaxed);
    m_outSize.store(outSize, std::memory_order_relaxed);
}

void RxDspWorker::processIqBatch(int receiverIndex,
                                 const QVector<float>& interleavedIQ)
{
    Q_UNUSED(receiverIndex);

    // Snapshot the sizing for this batch so a concurrent
    // setBufferSizes() (e.g. mid-batch reconfigure) can't split a
    // single drain across two values. The fields are std::atomic<int>
    // to avoid the C++ data race that a plain int read would hit; the
    // local snapshot then gives a stable pair for the rest of the batch.
    const int inSize  = m_inSize.load(std::memory_order_relaxed);
    const int outSize = m_outSize.load(std::memory_order_relaxed);

    // Deinterleave and append to accumulation buffers. Done regardless
    // of WDSP wiring so the chunkDrained signal can be observed in
    // tests that don't link a real WDSP build.
    const int numSamples = interleavedIQ.size() / 2;
    m_iqAccumI.reserve(m_iqAccumI.size() + numSamples);
    m_iqAccumQ.reserve(m_iqAccumQ.size() + numSamples);
    for (int i = 0; i < numSamples; ++i) {
        m_iqAccumI.append(interleavedIQ[i * 2]);
        m_iqAccumQ.append(interleavedIQ[i * 2 + 1]);
    }

    // Drain whole chunks of inSize through WDSP (or skip the WDSP/audio
    // calls when engines aren't wired — chunkDrained still fires so the
    // chunking contract is observable).
    while (m_iqAccumI.size() >= inSize) {
        if (m_wdspEngine != nullptr && m_audioEngine != nullptr) {
            RxChannel* rxCh = m_wdspEngine->rxChannel(0);
            if (rxCh == nullptr) {
                m_iqAccumI.clear();
                m_iqAccumQ.clear();
                emit batchProcessed();
                return;
            }

            QVector<float> outI(inSize);
            QVector<float> outQ(inSize);
            rxCh->processIq(m_iqAccumI.data(), m_iqAccumQ.data(),
                            outI.data(), outQ.data(), inSize, outSize);

            // WDSP outputs out_size samples per fexchange2 call.
            // outI == outQ because SetRXAPanelBinaural(channel, 0) puts
            // the RXA patch panel in dual-mono mode (set in
            // WdspEngine::createRxChannel).
            //
            // Phase 3O: interleave L/R into a reusable stereo buffer
            // and hand it to AudioEngine::rxBlockReady, which routes
            // through MasterMixer → speakers IAudioBus + per-slice VAX.
            // sliceId = 0 because ReceiverManager maps hardware DDC2 →
            // receiver index 0 for the single-RX live path (3O
            // Sub-Phase 4; multi-slice enrollment lands in Sub-Phase 9+).
            if (m_interleavedOut.size() < outSize * 2) {
                m_interleavedOut.resize(outSize * 2);
            }
            float* interleaved = m_interleavedOut.data();
            for (int i = 0; i < outSize; ++i) {
                interleaved[i * 2 + 0] = outI[i];
                interleaved[i * 2 + 1] = outQ[i];
            }
            m_audioEngine->rxBlockReady(0, interleaved, outSize);
        }

        m_iqAccumI.remove(0, inSize);
        m_iqAccumQ.remove(0, inSize);
        emit chunkDrained(inSize);
    }

    emit batchProcessed();
}

void RxDspWorker::resetAccumulator()
{
    m_iqAccumI.clear();
    m_iqAccumQ.clear();
}

} // namespace NereusSDR
