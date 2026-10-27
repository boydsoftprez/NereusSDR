//=================================================================
// SDRSerialPort.cs
//=================================================================
// Copyright (C) 2005  Bill Tracey
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
//================================================================= 
// Serial port support for PowerSDR support of CAT and serial port control  
//=================================================================

// Ported from Thetis Project Files/Source/Console/CAT/SDRSerialPortII.cs
// Modification history (NereusSDR):
// 2026-10-26 - CW transmit, the serial key reader (R-CW-04, R-CW-36):
//              SerialPinChanged's key, PTT and paddle lines read on a thread
//              of their own, edge driven (Linux TIOCMIWAIT, Windows
//              WaitCommEvent, macOS a 1 ms TIOCMGET poll), keying the host
//              key line. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
#pragma once

#include "core/CwTypes.h"
#include "core/NereusCoreExport.h"

#include <QObject>
#include <QString>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace NereusSDR {

class CwKeyLine;

// R-CW-04 and section 6: the words a serial key port is refused or lost
// with. Each passes OperatorWording::isPlain (tst_cw_serial_key_input).
inline constexpr char kCwSerialPortInUseByCatText[] = "That port is in use by CAT.";
inline constexpr char kCwSerialPortInUseByCatPttText[] =
    "That port is in use by the CAT PTT input.";
inline constexpr char kCwSerialPortOpenFailedText[] = "The port %1 could not be opened.";
inline constexpr char kCwSerialPortLostText[] = "The serial key port stopped working.";

// The serial key reader (R-CW-36, D14): DSR and CTS of one serial port, read
// on a thread of its own (ThreadRole::CwSerialKey) that waits for an edge.
//
// ---- Lines ----
// As Thetis SerialPinChanged (SDRSerialPortII.cs:246-263): with a key line
// or a PTT line chosen, the PTT line posts pttLineChanged (Thetis
// CWInput.KeyerPTT; RadioModel hands it to MoxController::onCwPtt, which
// keys MOX in Semi only and reports a refusal) and the key line keys the
// CWX bit through the straight key sink (Thetis NetworkIO.SetCWX; the CWX
// sender owns that bit). Otherwise, with paddles on, DSR is the dot and CTS
// the dash, written to the host key line's dot and dash bits (Thetis
// NetworkIO.SetCWDot and SetCWDash) for the radio's keyer.
//
// ---- The gate, stops and fresh edges ----
// Key bits (dot, dash, the straight key) are written only while the gate
// is allowed (setGateAllowed; RadioModel follows the CW keyer gate,
// R-CW-05). A gate turning false, releaseKeys (every stop and the transmit
// cut's catch-up, R-CW-08) and a key line change release the bits this
// reader holds, and releaseKeys also posts the PTT line up. A line that
// was down then, or that went down while the gate refused it, keys nothing
// until it has gone up and down again: nothing re-keys when a cut clears.
// A line already down when the port opens needs a fresh edge too.
//
// ---- Threads ----
// Every public method may be called from any thread; RadioModel calls them
// on the main thread. Key line writes happen under this object's mutex, so
// after setKeyLine or stopReading returns nothing writes the old line
// again. The signals are posted to this object's thread in the order the
// edges happened, so a release always lands after the press it follows.
class NEREUS_CORE_EXPORT CwSerialKeyInput : public QObject {
    Q_OBJECT

public:
    // The line source is a seam: production opens the port, tests inject edges.
    struct LineSource {
        virtual ~LineSource() = default;
        virtual bool open(const QString& port, QString* error) = 0;
        // Blocks until DSR or CTS changes or stop() is called; returns false on stop.
        virtual bool waitForChange(bool* dsr, bool* cts) = 0;
        virtual void stop() = 0;
        // NereusSDR addition: the levels when the port opened (true is
        // down, asserted). False when they are not known; both read up.
        virtual bool initialLevels(bool* dsr, bool* cts)
        {
            *dsr = false;
            *cts = false;
            return false;
        }
    };

    // Takes the key down (true) or up (false); returns false when the key
    // was not taken down (CwxSender::setHandKey).
    using StraightKeySink = std::function<bool(bool down)>;

    // The platform's own line source (D14): Linux TIOCMIWAIT, Windows
    // WaitCommEvent (EV_DSR | EV_CTS), macOS a 1 ms TIOCMGET poll.
    static std::unique_ptr<LineSource> createPlatformLineSource();

    explicit CwSerialKeyInput(std::unique_ptr<LineSource> source, QObject* parent = nullptr);
    ~CwSerialKeyInput() override;                  // stops and joins its thread

    // Opens `port` and starts reading it (a reader already running stops
    // first). False with `error` set to plain words when it cannot open.
    bool start(const QString& port, bool paddles, CwSerialLine keyLine,
               CwSerialLine pttLine, QString* error);
    // Stops and joins the thread, releases every bit and posts the PTT
    // line up when it was down. Nothing when not reading.
    void stopReading();
    void setKeyLine(std::shared_ptr<CwKeyLine> line);
    void setStraightKeySink(StraightKeySink sink);
    // R-CW-05: false releases the key bits; true keys nothing until a
    // fresh edge. Starts false.
    void setGateAllowed(bool allowed);
    // R-CW-08 and every stop: releases the key bits and the PTT line.
    void releaseKeys();

    bool isReading() const { return m_reading.load(std::memory_order_acquire); }
    // A dot, dash, straight key or PTT line this reader holds down now.
    bool isKeying() const { return m_keying.load(std::memory_order_acquire); }
    // A paddle (dot or dash line, paddles on) is down now, keyed or not.
    bool paddleDown() const { return m_paddleDown.load(std::memory_order_acquire); }

signals:
    void pttLineChanged(bool down);     // queued to the main thread
    void paddleActivity();              // for the auto mode switch
    void failed(const QString& words);

private:
    enum Line : int { kDsr = 0, kCts = 1 };

    void run();
    void onLevelsLocked(bool dsr, bool cts);
    void onEdgeLocked(Line line, bool down);
    void setPttLocked(Line line, bool down);
    void setStraightLocked(Line line, bool down);
    void setPaddleLocked(Line line, bool down);
    void releaseKeyBitsLocked();
    void releaseAllLocked();
    void updateKeyingLocked();
    bool lineIs(CwSerialLine chosen, Line line) const;

    std::unique_ptr<LineSource> m_source;
    std::thread m_thread;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_reading{false};
    std::atomic<bool> m_keying{false};
    std::atomic<bool> m_paddleDown{false};
    std::atomic<bool> m_gateAllowed{false};

    mutable std::mutex m_mutex;
    // Under m_mutex.
    std::shared_ptr<CwKeyLine> m_line;
    StraightKeySink m_straightSink;
    bool m_paddles{false};
    CwSerialLine m_keyLine{CwSerialLine::None};
    CwSerialLine m_pttLine{CwSerialLine::None};
    std::array<bool, 2> m_level{};      // DSR, CTS as last read (true: down)
    std::array<bool, 2> m_blocked{};    // keys nothing until it goes up
    bool m_dotOn{false};                // what this reader holds down
    bool m_dashOn{false};
    bool m_straightOn{false};
    bool m_pttOn{false};
};

} // namespace NereusSDR
