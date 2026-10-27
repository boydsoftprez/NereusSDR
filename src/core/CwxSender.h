// =================================================================
// src/core/CwxSender.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/cwx.cs (the CWX element engine
//     process_element, the default Morse table and build_mbits2,
//     loadchar, loadmsg, queue_start, setptt, setkey, quitshut and the
//     Key's KeyAction)
//   Project Files/Source/Console/TCIServer.cs (prosigns between bars, the
//     '>' and '<' speed marks, the 3 s key watchdog)
//   original licence from Thetis source is included below
//
// Ported from mi0bot-Thetis sources:
//   Project Files/Source/Console/cwx.cs (setptt drives the HL2's CWX PTT)
//   original licence from mi0bot-Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-27: CW transmit, the CWX sender (R-CW-15, R-CW-31 to
//               R-CW-33): reimplemented in C++20/Qt6 for NereusSDR on a
//               thread of its own, keying the host key line. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-27: CW transmit, the TCI keyer's timed release
//               (releaseKeyAfter, from TCIServer.cs scheduleKeyerRelease),
//               isKeyHeld and keyHeldChanged (R-CW-35). J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-26: CW transmit, setHandKey, the serial straight-key line on
//               the CWX bit (R-CW-36). J.J. Boyd (KG4VCF), AI-assisted via
//               Claude Code.
//   2026-10-26: CW transmit, setKeyListener, the sender's own key for the
//               computer sidetone (R-CW-39). J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

// --- From [cwx.cs] ---
//=================================================================
// cwx.cs
//=================================================================
// CWX - new version of the old keyer memory and keyboard stuff
// Copyright (C) 2004-2012  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================
//
//         CWX written by Richard Allen, W5SXD
//    with various hooks from Eric Wachsman, KE5DTO, 
//          and Herr Doktor Bob McGwier, N4HY
//            November 2005 - February 2006
//
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

// --- From [cwx.cs (mi0bot)] ---
//=================================================================
// cwx.cs
//=================================================================
// CWX - new version of the old keyer memory and keyboard stuff
// Copyright (C) 2004-2012  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================
//
//         CWX written by Richard Allen, W5SXD
//    with various hooks from Eric Wachsman, KE5DTO, 
//          and Herr Doktor Bob McGwier, N4HY
//            November 2005 - February 2006
//
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

// --- From [TCIServer.cs] ---
/*  TCIServer.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

mw0lge@grange-lane.co.uk
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

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "core/NereusCoreExport.h"

namespace NereusSDR {

class CwKeyLine;

// R-CW-32 and section 6: the words a CWX request is refused with. Each
// passes OperatorWording::isPlain (tst_cwx_sender).
inline constexpr char kCwxNotCwText[] = "Switch the slice to CW to send.";
inline constexpr char kCwxNothingToSendText[] = "There is nothing to send.";
inline constexpr char kCwxMemoryEmptyText[] = "Memory %1 is empty.";
inline constexpr char kCwxNoSuchMemoryText[] = "There is no memory %1.";

// The CWX sender (R-CW-15, R-CW-31 to R-CW-33): CW text and memories keyed
// on the host key line (CwKeyLine's CWX bit, and on the Hermes Lite 2 its
// CWX PTT bit) with Thetis's CWX element engine, on a thread of its own.
//
// ---- Timing ----
// One element is 1200 / wpm ms (whole ms, as Thetis's wpmrate). The thread
// runs Thetis's process_element once per element, and every element
// boundary is the previous one plus one element, counted from the start of
// the message, so nothing drifts. A send while the sender is idle starts
// a new count at once.
//
// ---- Requests ----
// sendText, sendMemory and setKeyHeld(true) ask the request gate first
// (setRequestGate; on the caller's thread), then refuse what has nothing
// to send. A refusal returns false and emits refused(words) at once.
// sendText appends to what is being sent; sendMemory and the Key replace
// it. stop() aborts and clears. Every request, stop and setter may be
// called from any thread; RadioModel calls them on the main thread, where
// its gate reads the models.
//
// ---- The key line ----
// Writes happen under the sender's mutex, on its thread for elements and on
// the caller's thread for stop(), the Key and the setters, so after
// setKeyLine (any line, null included) returns, nothing writes the old
// line again. setKeyLine releases what the sender holds on the old line and
// aborts anything being sent. The CWX bit (and on the HL2 the CWX PTT bit)
// is written only while break-in is active or MOX is held (R-CW-32: in Off
// CWX keys only while MOX is held); the element timing runs either way.
//
// ---- Signals ----
// characterStarted, sendingChanged and emptied are posted to this object's
// thread in the order they happen; refused is emitted on the caller's
// thread at once.
class NEREUS_CORE_EXPORT CwxSender : public QObject {
    Q_OBJECT

public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using Sleeper = std::function<void(std::chrono::steady_clock::time_point until)>;
    // Empty when a request may key; otherwise the words it is refused with.
    using RequestGate = std::function<QString()>;

    // D12: CWX's own speed and repeat delay (TransmitModel's ranges).
    static constexpr int kMinWpm = 5;
    static constexpr int kMaxWpm = 60;
    static constexpr int kDefaultWpm = 22;
    static constexpr int kMinRepeatDelaySeconds = 1;
    static constexpr int kMaxRepeatDelaySeconds = 60;
    static constexpr int kDefaultRepeatDelaySeconds = 3;
    // D11: eight memories.
    static constexpr int kMemoryCount = 8;

    // From Thetis cwx.cs:404-407 [v2.10.3.15]: wpmrate, 1200 / cwxwpm
    // ("Tel in ms from wpm (based on PARIS method").
    static constexpr int kElementMsAtOneWpm = 1200;
    // From Thetis cwx.cs:1381-1385 [v2.10.3.15]: udPtt's value (hidden in
    // Thetis's form), the PTT to first key down delay in ms.
    static constexpr int kPttDelayMs = 50;
    // From Thetis cwx.cs:1430-1434 [v2.10.3.15]: udDrop's value (hidden in
    // Thetis's form), the time PTT is held after a key down in ms.
    static constexpr int kDropDelayMs = 300;
    // From Thetis TCIServer.cs:8233 [v2.10.3.15]: DirectKeyerWatchdogMs, a
    // key held with no release is dropped this long after its last press.
    static constexpr int kKeyWatchdogMs = 3000;
    // From Thetis TCIServer.cs:8745-8749 [v2.10.3.15]: '>' and '<' change
    // the speed by 5 WPM.
    static constexpr int kSpeedMarkStepWpm = 5;

    explicit CwxSender(QObject* parent = nullptr);
    CwxSender(Clock clock, Sleeper sleeper, QObject* parent = nullptr); // tests
    ~CwxSender() override;                         // stops and joins its thread

    void setKeyLine(std::shared_ptr<CwKeyLine> line);
    void setHl2(bool hl2);                         // drive cwxPtt (R-CW-15)
    void setSpeedWpm(int wpm);                     // 5..60
    void setRepeat(bool on, int delaySeconds);     // 1..60
    void setBreakInActive(bool on);                // false in Off
    void setMoxActive(bool on);
    // R-CW-33: asked before every send and Key press. Unset: allowed.
    void setRequestGate(RequestGate gate);
    // Returns false with words when refused (not CW, or nothing to send).
    bool sendText(const QString& text);
    bool sendMemory(int number);                   // 1..8, using the memory texts
    void setMemories(const QStringList& eight);
    void stop();                                   // abort and clear
    void setKeyHeld(bool down);                    // the Key hand key, 3 s watchdog
    // R-CW-35, the TCI keyer: the held Key goes up `msAfterPress` ms after
    // it went down (at once when that has passed), never later than the
    // watchdog. Nothing when the Key is not held.
    // From Thetis TCIServer.cs:9050-9064 [v2.10.3.15]
    // (scheduleKeyerReleaseLocked).
    void releaseKeyAfter(int msAfterPress);
    // R-CW-36, the serial straight-key line (Thetis SDRSerialPortII.cs:251
    // and 256, NetworkIO.SetCWX): the CWX bit held while `down`, with no
    // request gate (the serial reader applies the keyer gate on its own
    // thread) and no watchdog (a hand key may be held as long as the
    // operator holds it). Ignored while text is sent or the Key is held,
    // as the TCI key is. Returns true when the key is down after the call.
    // Any thread; stop() and setKeyLine release it.
    bool setHandKey(bool down);
    // R-CW-39: the computer sidetone's feed. Told the sender's own key (a
    // message's elements, the Key, the TCI key and the straight key) on
    // each change, whether or not the line may carry it (in Off without
    // MOX the line is left alone, the sidetone still sounds). Called on the
    // thread that changed the key, under the sender's lock, so it must only
    // store an atomic. Set it before keying.
    using KeyListener = std::function<void(bool down)>;
    void setKeyListener(KeyListener listener);
    bool isKeyHeld() const { return m_keyHeldNow.load(std::memory_order_acquire); }
    int queuedCharacters() const;                  // CAT KY/ZZKY buffer answers
    // A message, its hang or a repeat delay is running, or the Key is held.
    bool isSending() const { return m_sending.load(std::memory_order_acquire); }

    // The element time at `wpm` (clamped to 5..60), in ms.
    static int elementMs(int wpm);

signals:
    void characterStarted(int index);              // for the applet's sent/current text
    void sendingChanged(bool sending);
    void emptied();                                // TCI cw_macros_empty
    void refused(const QString& words);
    void keyHeldChanged(bool held);                // the Key went down or up

private:
    using TimePoint = std::chrono::steady_clock::time_point;

    // One entry of Thetis's element fifo.
    struct Element {
        quint8 code{0};
        qint8 speedDelta{0};     // WPM from '>' and '<', added to the speed
        int charIndex{-1};       // index in the request's text
        bool firstOfChar{false}; // the character starts with this element
    };

    void run();
    void tickLocked();
    void releaseKeyHeldLocked();
    void setKeyHeldStateLocked(bool held);
    void abortLocked();
    void applyLineLocked();
    void setPttLocked(bool on);
    void setKeyLocked(bool on);
    bool reloadRepeatLocked();
    void startTickingLocked();
    void setSendingLocked(bool on);
    void finishIfIdleLocked();
    void preciseSleepUntil(TimePoint until);
    int currentElementMsLocked() const;
    QString gateWords() const;

    // Text to elements: sendText follows Thetis's loadchar (keyboard and
    // CAT), a memory its loadmsg (the '"' loop, '#' and '$').
    std::deque<Element> elementsForText(const QString& text, int baseWpm, int* characters) const;
    std::deque<Element> elementsForMemory(const QString& text, int baseWpm, int* characters,
                                          bool* loops) const;

    Clock m_clock;
    Sleeper m_sleeper;
    RequestGate m_gate;
    std::array<quint32, 64> m_mbits{};

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_wake{false};
    bool m_quit{false};

    // Under m_mutex.
    std::shared_ptr<CwKeyLine> m_line;
    bool m_hl2{false};
    bool m_breakIn{false};
    bool m_mox{false};
    bool m_lineKey{false};       // what this sender last wrote
    bool m_linePtt{false};
    int m_speedWpm{kDefaultWpm};
    bool m_repeatOn{false};
    int m_repeatDelaySeconds{kDefaultRepeatDelaySeconds};
    QStringList m_memories;

    std::deque<Element> m_queue;
    bool m_ticking{false};
    TimePoint m_nextTick{};
    int m_currentDelta{0};
    // Thetis's engine state (process_element).
    bool m_ptt{false};
    bool m_key{false};
    int m_ttx{0};
    int m_pause{0};
    int m_newptt{0};
    // The memory being repeated, empty when none.
    QString m_repeatText;
    bool m_repeatMarker{false};
    // The Key (setKeyHeld).
    bool m_keyHeld{false};
    TimePoint m_keyDeadline{};
    TimePoint m_keyPressedAt{};
    std::atomic<bool> m_keyHeldNow{false};
    // The serial straight key (setHandKey).
    bool m_handKey{false};
    // The key last told to m_keyListener (setKeyListener).
    KeyListener m_keyListener;
    bool m_listenedKey{false};

    std::atomic<int> m_queuedChars{0};
    std::atomic<bool> m_sending{false};
    bool m_messageQueued{false};   // something was queued since the last emptied

    std::thread m_thread;
};

} // namespace NereusSDR
