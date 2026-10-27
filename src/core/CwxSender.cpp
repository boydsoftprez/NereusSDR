// =================================================================
// src/core/CwxSender.cpp  (NereusSDR)
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

#include "core/CwxSender.h"

#include "core/CwKeyLine.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/platform/ThreadPlacement.h"

#include <QMetaObject>

#include <algorithm>
#include <utility>
#include <vector>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <pthread.h>
#endif
#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace NereusSDR {

namespace {

// From Thetis cwx.cs:72-77 [v2.10.3.15]: the element codes.
constexpr quint8 kElPtt = 0x10;       // extra ptt delay command
constexpr quint8 kElPause = 0x08;     // pause flag
constexpr quint8 kElEnd = 0x4;        // end flag
constexpr quint8 kElKeyUp = 0x2;      // key up flag
constexpr quint8 kElKeyDown = 0x3;    // key down

// From Thetis cwx.cs:535-598 [v2.10.3.15]: the default morsedef.txt, which
// Thetis writes once and reads back into a2m2 (the user may edit the file
// there; NereusSDR keeps the defaults). In ASCII order from 32 to 95.
constexpr std::array<const char*, 64> kMorseDef = {{
    "32| |*        | space     ",
    "33|!|...-.    | [SN]      ",
    "34|\"|*        | loop      ",
    "35|#|*        | long dash ",
    "36|$|*        | long space",
    "37|%|.-...    | [AS]      ",
    "38|&|.........| 0123456789",
    "39|'|         |           ",
    "40|(|-.--.    | [KN]      ",
    "41|)|         |           ",
    "42|*|...-.-   | [SK]      ",
    "43|+|.-.-.    | [AR]      ",
    "44|,|--..--   |           ",
    "45|-|-....-   |           ",
    "46|.|.-.-.-   |           ",
    "47|/|-..-.    |           ",
    "48|0|-----    |           ",
    "49|1|.----    |           ",
    "50|2|..---    |           ",
    "51|3|...--    |           ",
    "52|4|....-    |           ",
    "53|5|.....    |           ",
    "54|6|-....    |           ",
    "55|7|--...    |           ",
    "56|8|---..    |           ",
    "57|9|----.    |           ",
    "58|:|         |           ",
    "59|;|         |           ",
    "60|<|         |           ",
    "61|=|-...-    | [BT]      ",
    "62|>|         |           ",
    "63|?|..--..   |           ",
    "64|@|.--.-.   |           ",
    "65|A|.-       |           ",
    "66|B|-...     |           ",
    "67|C|-.-.     |           ",
    "68|D|-..      |           ",
    "69|E|.        |           ",
    "70|F|..-.     |           ",
    "71|G|--.      |           ",
    "72|H|....     |           ",
    "73|I|..       |           ",
    "74|J|.---     |           ",
    "75|K|-.-      |           ",
    "76|L|.-..     |           ",
    "77|M|--       |           ",
    "78|N|-.       |           ",
    "79|O|---      |           ",
    "80|P|.--.     |           ",
    "81|Q|--.-     |           ",
    "82|R|.-.      |           ",
    "83|S|...      |           ",
    "84|T|-        |           ",
    "85|U|..-      |           ",
    "86|V|...-     |           ",
    "87|W|.--      |           ",
    "88|X|-..-     |           ",
    "89|Y|-.--     |           ",
    "90|Z|--..     |           ",
    "91|[|         |           ",
    "92|\\|-...-.-  | [BK]      ",
    "93|]|         |           ",
    "94|^|         |           ",
    "95|_|*        | reserved  ",
}};

// From Thetis cwx.cs:466-521 [v2.10.3.15]: build_mbits2, the Morse element
// maps from the table. Each map's top bits are the marks of the character
// and its spaces, one bit per element; the low five bits are its element
// count.
std::array<quint32, 64> buildMbits()
{
    std::array<quint32, 64> mbits{};
    for (int i = 0; i < 64; i++)	// for each code
    {
        quint32 nel = 0;
        quint32 els = 0;
        quint32 mask = 0x80000000;
        /*
        012345678901234
        64|@|.--.-.   |
        */

        const QByteArray st(kMorseDef[static_cast<size_t>(i)]);
        const QByteArray s = st.mid(5, 9);

        const int ndd = static_cast<int>(s.size());
        for (int k = 0; k < ndd; k++)
        {
            if (s.at(k) == '-')
            {
                nel += 4;
                els |= mask; mask >>= 1;
                els |= mask; mask >>= 1;
                els |= mask; mask >>= 1;
                mask >>= 1;
            }
            else if (s.at(k) == '.')
            {
                nel += 2;
                els |= mask; mask >>= 1;
                mask >>= 1;
            }
            else
            {
                continue;
            }
        }
        // a space will have a 4 element count + 1 element space + 2 letter spaces
        // making a total of 7 elements in a word space

        if (i == 0) {
            nel = 4;	// value for ' ' character
        } else {
            nel += 2;			// add letter space
        }

        els &= 0xffffffe0;
        els += nel & 0x1f;
        mbits[static_cast<size_t>(i)] = els;
    }
    return mbits;
}

// One character of a request after the prosigns and speed marks are read:
// the character, its index in the request's text, and the speed change in
// force (WPM, from '>' and '<').
struct Piece {
    QChar ch;
    int index{0};
    int speedDelta{0};
};

// From Thetis TCIServer.cs:8659-8676 [v2.10.3.15]: translateAbbreviationToken,
// a prosign between bars to the character that sends it.
QString translateAbbreviationToken(const QString& token)
{
    const QString value = token;
    const QString key = value.trimmed().toUpper();
    if (key == QLatin1String("SK")) { return QStringLiteral("*"); }
    if (key == QLatin1String("AR")) { return QStringLiteral("+"); }
    if (key == QLatin1String("KN")) { return QStringLiteral("("); }
    if (key == QLatin1String("SN")) { return QStringLiteral("!"); }
    if (key == QLatin1String("BT")) { return QStringLiteral("="); }
    if (key == QLatin1String("BK")) { return QStringLiteral("\\"); }
    if (key == QLatin1String("AS")) { return QStringLiteral("%"); }
    return value;
}

int clampWpm(int wpm)
{
    return std::clamp(wpm, CwxSender::kMinWpm, CwxSender::kMaxWpm);
}

// From Thetis TCIServer.cs:8703-8760 [v2.10.3.15]: parseMacroText, the
// prosigns between bars and the '>' and '<' speed marks. Thetis clamps the
// speed to 1..99 (clampMacroSpeed, TCIServer.cs:8642-8645); NereusSDR clamps
// it to CWX's own 5..60 (D12). Thetis splits the text into segments, each
// sent at its own speed; here each character carries its speed change.
std::vector<Piece> parseMacroText(const QString& text, int baseWpm)
{
    std::vector<Piece> pieces;
    int speed = clampWpm(baseWpm);
    const int base = speed;
    for (int i = 0; i < text.size(); i++)
    {
        const QChar ch = text.at(i);

        if (ch == QLatin1Char('|') && i + 1 < text.size())
        {
            const int end = text.indexOf(QLatin1Char('|'), i + 1);
            if (end > i + 1)
            {
                const QString translated = translateAbbreviationToken(text.mid(i + 1, end - i - 1));
                for (const QChar t : translated) {
                    pieces.push_back(Piece{t, i, speed - base});
                }
                i = end;
                continue;
            }
        }

        if (ch == QLatin1Char('>') || ch == QLatin1Char('<'))
        {
            speed = clampWpm(speed + (ch == QLatin1Char('>') ? CwxSender::kSpeedMarkStepWpm
                                                             : -CwxSender::kSpeedMarkStepWpm));
            continue;
        }

        pieces.push_back(Piece{ch, i, speed - base});
    }
    return pieces;
}

void nameThisThread()
{
#if defined(Q_OS_MAC)
    pthread_setname_np("NereusCwxSender");
#elif defined(Q_OS_LINUX)
    // Linux allows 15 characters plus the terminator.
    pthread_setname_np(pthread_self(), "NereusCwxSender");
#endif
}

#if defined(Q_OS_WIN)
// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION (Windows 10, version 1803 and
// later). An older Windows refuses it, and the nap falls back to a yield.
constexpr DWORD kHighResolutionTimerFlag = 0x00000002;

class HighResolutionNap {
public:
    HighResolutionNap()
        : m_timer(CreateWaitableTimerExW(nullptr, nullptr, kHighResolutionTimerFlag,
                                         TIMER_ALL_ACCESS))
    {}
    ~HighResolutionNap()
    {
        if (m_timer != nullptr) {
            CloseHandle(m_timer);
        }
    }
    HighResolutionNap(const HighResolutionNap&) = delete;
    HighResolutionNap& operator=(const HighResolutionNap&) = delete;

    bool nap(std::chrono::microseconds length)
    {
        if (m_timer == nullptr) {
            return false;
        }
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>(length.count() * 10);   // 100 ns units, relative
        if (!SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE)) {
            return false;
        }
        WaitForSingleObject(m_timer, INFINITE);
        return true;
    }

private:
    HANDLE m_timer{nullptr};
};

// The condition variable's timeout follows the system timer (15.6 ms
// unless raised), so the coarse wait stops well short of the deadline and
// the high resolution timer naps the rest.
constexpr auto kCoarseMargin = std::chrono::milliseconds(20);
constexpr auto kNapMax = std::chrono::milliseconds(2);
#elif defined(Q_OS_MAC)
// macOS gives a timed condition wait up to 10 ms of timer leeway, even at
// USER_INTERACTIVE QoS (measured 2026-10-27 on the M-series bench: 10.0 ms
// late for 38 ms to 3 s waits, 2.5 ms for 5 ms waits), while a 500 us
// sleep wakes within about 0.5 ms. So the coarse wait stops 15 ms short and
// short sleeps cover the rest.
constexpr auto kCoarseMargin = std::chrono::milliseconds(15);
constexpr auto kNapMax = std::chrono::microseconds(500);
#else
constexpr auto kCoarseMargin = std::chrono::milliseconds(2);
constexpr auto kNapMax = std::chrono::microseconds(500);
#endif
// The last stretch before a deadline is a yield loop.
constexpr auto kSpinWindow = std::chrono::milliseconds(1);

void shortNap(std::chrono::microseconds length)
{
#if defined(Q_OS_WIN)
    static thread_local HighResolutionNap timer;
    if (!timer.nap(length)) {
        std::this_thread::yield();
    }
#else
    std::this_thread::sleep_for(length);
#endif
}

} // namespace

// ---------------------------------------------------------------------------
// Construction and the thread
// ---------------------------------------------------------------------------

CwxSender::CwxSender(QObject* parent)
    : CwxSender(Clock{}, Sleeper{}, parent)
{
}

CwxSender::CwxSender(Clock clock, Sleeper sleeper, QObject* parent)
    : QObject(parent)
    , m_clock(std::move(clock))
    , m_sleeper(std::move(sleeper))
    , m_mbits(buildMbits())
{
    if (!m_clock) {
        m_clock = []() { return std::chrono::steady_clock::now(); };
    }
    if (!m_sleeper) {
        m_sleeper = [this](TimePoint until) { preciseSleepUntil(until); };
    }
    for (int i = 0; i < kMemoryCount; ++i) {
        m_memories.append(QString());
    }
    m_thread = std::thread([this] { run(); });
}

CwxSender::~CwxSender()
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        abortLocked();
        m_quit = true;
        m_wake.store(true, std::memory_order_release);
    }
    m_cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void CwxSender::run()
{
    nameThisThread();
    // As the P1 EP2 sender: placed by ThreadPlacement where it manages
    // thread priority (nereusd), otherwise raised for latency.
    const bool placed = ThreadPlacement::managesThreadPriority();
    AudioPriorityToken* prio = nullptr;
    if (placed) {
        ThreadPlacement::instance().registerCurrentThread(ThreadRole::CwxSender);
    } else {
#if defined(Q_OS_MAC)
        elevateLatencyCriticalThreadPriority(/*logSuccess=*/false);
#else
        prio = elevateAudioThreadPriority();
#endif
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_quit) {
        if (!m_ticking && !m_keyHeld) {
            m_cv.wait(lock, [this] { return m_quit || m_ticking || m_keyHeld; });
            continue;
        }
        const TimePoint deadline = m_keyHeld ? m_keyDeadline : m_nextTick;
        if (m_clock() < deadline) {
            m_wake.store(false, std::memory_order_release);
            lock.unlock();
            m_sleeper(deadline);
            lock.lock();
            continue;
        }
        if (m_keyHeld) {
            // The Key's watchdog: no press for kKeyWatchdogMs.
            releaseKeyHeldLocked();
            continue;
        }
        tickLocked();
    }
    lock.unlock();

    if (placed) {
        ThreadPlacement::instance().deregisterCurrentThread();
    } else {
        leaveAudioThreadPriority(prio);
    }
}

// A wait on the condition variable until shortly before the deadline, short
// naps, then a yield loop for the last millisecond. A request, stop or
// shutdown ends it early.
void CwxSender::preciseSleepUntil(TimePoint until)
{
    using std::chrono::steady_clock;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        const TimePoint coarse = until - kCoarseMargin;
        if (steady_clock::now() < coarse) {
            m_cv.wait_until(lock, coarse, [this] {
                return m_quit || m_wake.load(std::memory_order_acquire);
            });
        }
        if (m_quit) {
            return;
        }
    }
    while (!m_wake.load(std::memory_order_acquire)) {
        const TimePoint now = steady_clock::now();
        if (now >= until) {
            return;
        }
        const auto left = until - now;
        if (left > kSpinWindow) {
            shortNap(std::chrono::duration_cast<std::chrono::microseconds>(
                std::min<steady_clock::duration>(left - kSpinWindow, kNapMax)));
        } else {
            std::this_thread::yield();
        }
    }
}

// ---------------------------------------------------------------------------
// The element engine
// ---------------------------------------------------------------------------

int CwxSender::elementMs(int wpm)
{
    // From Thetis cwx.cs:404-407 [v2.10.3.15]
    // Tel in ms from wpm (based on PARIS method
    return kElementMsAtOneWpm / clampWpm(wpm);
}

int CwxSender::currentElementMsLocked() const
{
    return elementMs(m_speedWpm + m_currentDelta);
}

// One element time. From Thetis cwx.cs:2187-2273 [v2.10.3.15], process_element:
//
// process_element is called at the element rate (width of dot)
// and pulls commands out of the element fifo.  These are basically
// to determine the state of the key during the next element time.
// The timeout pause is also processed here.
//
// NereusSDR: Thetis's quit flag and checkPTT are not read here: stop()
// does quitshut's work at once on the caller's thread, and every request
// passes the request gate before it is queued. An end command does not run
// quitshut: the key and PTT fall with the hang below, as they do after
// typed text, so the HL2's CWX PTT is held for the hang after the last
// element (R-CW-15). The next element starts one element time after this
// one, whatever this one did.
void CwxSender::tickLocked()
{
    const auto processElement = [this]() {
        const int tel = currentElementMsLocked();
        if (m_newptt > 0)
        {
            m_newptt--;
            m_ttx = kDropDelayMs / tel;
            if (m_newptt > 0) {
                return;
            }
            //				Debug.WriteLine("newppt delay over");
            setKeyLocked(true);				// this was the defered key down
            return;
        }
        if (m_pause > 0)	// time out the pause
        {
            m_pause--;
            if (m_pause > 0) {
                return;		// not yet done
            }
            // pause ended, load 'er up again
            reloadRepeatLocked();
            return;
        }
        while (true)		// used while for control; only once ever thru
        {
            if (m_queue.empty()) {
                break;
            }
            const Element data = m_queue.front();
            m_queue.pop_front();
            m_currentDelta = data.speedDelta;
            const int elTel = currentElementMsLocked();
            if (data.firstOfChar) {
                m_queuedChars.fetch_sub(1, std::memory_order_acq_rel);
                const int index = data.charIndex;
                QMetaObject::invokeMethod(this, [this, index]() { emit characterStarted(index); },
                                          Qt::QueuedConnection);
            }
            if (data.code == kElEnd)		// end command
            {
                m_repeatText.clear();
                m_repeatMarker = false;
                break;
            }
            if (data.code == kElPause)		// pause command
            {
                // NereusSDR: a pause the Rpt switch put there is an end
                // once the switch is off.
                if (!m_repeatMarker && !m_repeatOn) {
                    m_repeatText.clear();
                    break;
                }
                m_ttx = 0;
                m_pause = (m_repeatDelaySeconds * 1000) / elTel;
                if (m_pause < 1) {
                    m_pause = elTel;
                }
                break;
            }
            if (data.code == kElPtt)		// ptt only command
            {
                setPttLocked(true);
                m_ttx = kDropDelayMs / elTel;
            }
            if ((data.code == kElKeyDown) || (data.code == kElKeyUp))		// key command
            {
                if (data.code == kElKeyDown)	// key down?
                {
                    if (!m_ptt)	// we're gonna need a ptt->key delay setup
                    {
                        m_newptt = kPttDelayMs / elTel;
                        //							Debug.WriteLine("start newptt");
                    }
                    setPttLocked(true);
                    m_ttx = kDropDelayMs / elTel;
                    if (m_newptt > 0) {
                        return;		// the key will get pressed after newptt
                    }
                    setKeyLocked(true);
                }
                else
                {
                    setKeyLocked(false);
                    break;
                }
            }
            return;		// ignore all others
        }
        // X on flow
        if (m_ttx > 0) {
            m_ttx--;			// time out timer down one element
        }
        if (m_ttx > 0) {
            return;		// not yet timed out
        }
        //[2.10.3]MW0LGE swap
        setKeyLocked(false);
        setPttLocked(false);			// cw timer timed out
    };

    processElement();

    // The next element boundary, from the previous one (no drift). NereusSDR:
    // a thread held off the processor for more than a whole element starts
    // a new count from now, rather than sending the elements it missed back
    // to back.
    const auto element = std::chrono::milliseconds(currentElementMsLocked());
    m_nextTick += element;
    const TimePoint now = m_clock();
    if (now > m_nextTick + element) {
        m_nextTick = now;
    }
    finishIfIdleLocked();
}

// From Thetis cwx.cs:2218-2221 [v2.10.3.15] (pause ended): loadmsg(tqq) again.
// Thetis reloads every message whose pause ran out; NereusSDR reloads one
// whose '"' marker loops it, or while the Rpt switch is on (D12).
bool CwxSender::reloadRepeatLocked()
{
    if (m_repeatText.isEmpty() || (!m_repeatMarker && !m_repeatOn)) {
        m_repeatText.clear();
        return false;
    }
    int characters = 0;
    bool loops = false;
    m_queue = elementsForMemory(m_repeatText, m_speedWpm, &characters, &loops);
    m_repeatMarker = loops;
    m_queuedChars.store(characters, std::memory_order_release);
    return true;
}

void CwxSender::finishIfIdleLocked()
{
    if (!m_queue.empty() || m_key || m_ptt || m_pause > 0 || m_newptt > 0 || m_ttx > 0) {
        return;
    }
    m_ticking = false;
    m_queuedChars.store(0, std::memory_order_release);
    if (m_messageQueued) {
        m_messageQueued = false;
        QMetaObject::invokeMethod(this, [this]() { emit emptied(); }, Qt::QueuedConnection);
    }
    if (!m_keyHeld) {
        setSendingLocked(false);
    }
}

void CwxSender::startTickingLocked()
{
    if (m_ticking) {
        return;
    }
    m_ticking = true;
    m_currentDelta = 0;
    m_nextTick = m_clock();
    setSendingLocked(true);
    m_wake.store(true, std::memory_order_release);
    m_cv.notify_all();
}

void CwxSender::setSendingLocked(bool on)
{
    if (m_sending.exchange(on, std::memory_order_acq_rel) != on) {
        QMetaObject::invokeMethod(this, [this, on]() { emit sendingChanged(on); },
                                  Qt::QueuedConnection);
    }
}

// From Thetis cwx.cs:310-319 [v2.10.3.15], quitshut, with the queue, the
// repeat and the Key cleared too. NetworkIO.SendHighPriority(1): a change of
// the key line wakes the Protocol 2 high priority send itself.
void CwxSender::abortLocked()
{
    m_queue.clear();
    m_repeatText.clear();
    m_repeatMarker = false;
    setKeyHeldStateLocked(false);
    m_handKey = false;
    setKeyLocked(false); //[2.10.3]MW0LGE swap
    setPttLocked(false);
    applyLineLocked();
    m_ttx = 0; m_pause = 0; m_newptt = 0;
    m_ticking = false;
    m_queuedChars.store(0, std::memory_order_release);
}

// From Thetis cwx.cs:277-294 [v2.10.3.15] (setptt) and mi0bot cwx.cs:288-290
// [v2.10.3.13-beta2] (the HL2's CWX PTT).
void CwxSender::setPttLocked(bool on)
{
    if (m_ptt != on)
    {
        //if (!console.CWFWKeyer)
        //{
        //    // CWPTTItem item = new CWPTTItem(state, CWSensorItem.GetCurrentTime());
        //    // CWKeyer.PTTEnqueue(item);
        //}

        m_ptt = on;
        applyLineLocked();
    }
    //			if (newptt) Thread.Sleep(200);
}

// From Thetis cwx.cs:297-308 [v2.10.3.15]: setkey, NetworkIO.SetCWX.
void CwxSender::setKeyLocked(bool on)
{
    if (m_key != on)
    {
        m_key = on;
        applyLineLocked();
    }
}

// The line follows the engine's key and PTT, while break-in is active or
// MOX is held (R-CW-32). The CWX PTT, HL2 only, goes up before the key and
// down after it, as setptt runs before setkey on a key down and after it
// on the way up (the [2.10.3]MW0LGE swap in quitshut and process_element).
void CwxSender::applyLineLocked()
{
    // R-CW-39: the computer sidetone follows the sender's own key, ahead of
    // the line's break-in or MOX rule and whether or not a line is set.
    const bool keyed = m_key || m_handKey;
    if (keyed != m_listenedKey) {
        m_listenedKey = keyed;
        if (m_keyListener) {
            m_keyListener(keyed);
        }
    }
    const bool allowed = m_breakIn || m_mox;
    const bool key = (m_key || m_handKey) && allowed;
    const bool ptt = m_hl2 && m_ptt && allowed;
    if (!m_line) {
        m_lineKey = false;
        m_linePtt = false;
        return;
    }
    if (ptt && !m_linePtt) {
        m_line->setCwxPtt(true);
        m_linePtt = true;
    }
    if (key != m_lineKey) {
        m_line->setCwx(key);
        m_lineKey = key;
    }
    if (!ptt && m_linePtt) {
        m_line->setCwxPtt(false);
        m_linePtt = false;
    }
}

void CwxSender::setKeyHeldStateLocked(bool held)
{
    m_keyHeld = held;
    if (m_keyHeldNow.exchange(held, std::memory_order_acq_rel) != held) {
        QMetaObject::invokeMethod(this, [this, held]() { emit keyHeldChanged(held); },
                                  Qt::QueuedConnection);
    }
}

void CwxSender::releaseKeyHeldLocked()
{
    setKeyHeldStateLocked(false);
    setKeyLocked(false);
    setPttLocked(false);
    if (!m_ticking) {
        setSendingLocked(false);
    }
}

// ---------------------------------------------------------------------------
// Text to elements
// ---------------------------------------------------------------------------

// From Thetis cwx.cs:2389-2420 [v2.10.3.15], loadchar, for each character
// after the prosigns and speed marks (TCI and CAT text reach Thetis's CWX
// this way, through RemoteMessage).
std::deque<CwxSender::Element> CwxSender::elementsForText(const QString& text, int baseWpm,
                                                           int* characters) const
{
    std::deque<Element> out;
    int count = 0;
    for (const Piece& piece : parseMacroText(text, baseWpm)) {
        // convert and load a single character
        // this is the guts of loadmsg and work much the same way
        const int cc = piece.ch.unicode();
        quint32 v = 0;
        int ic = cc - ' ';

        if ((cc >= 'a') && (cc <= 'z')) {
            ic -= 32;	// toupper case
        }

        if (ic < 0 || ic > 63) {
            continue;		// ignore bad codes
        }

        ic &= 0x3f;		// isolate the 0-63 code  (we did that in the previous line>)


        if (ic == 2) {
            continue;	// ignore loop back
        }
        if (ic == 3) {
            continue;	// ignore long dash
        }
        if (ic == 4) {
            continue;	//  and long space
        }

        v = m_mbits[static_cast<size_t>(ic)];	// look up code for the character
        // top 24 of v are marks, bottom 5 have count

        quint32 n = v & 0x1f;		// element count
        bool first = true;
        while (n > 0)	// push n marks and spaces to the element fifo
        {
            Element e;
            e.code = (v & 0x80000000) > 0 ? kElKeyDown : kElKeyUp;
            e.speedDelta = static_cast<qint8>(piece.speedDelta);
            e.charIndex = piece.index;
            e.firstOfChar = first;
            first = false;
            out.push_back(e);
            v <<= 1;
            n--;
        }
        ++count;
    }
    *characters = count;
    return out;
}

// From Thetis cwx.cs:2422-2496 [v2.10.3.15], loadmsg, for each character
// after the prosigns and speed marks. NereusSDR: a code outside 0..63 is
// skipped, where Thetis's & 0x3f turns it into another character; with the
// Rpt switch on (D12) a message without the '"' marker ends in a pause, as
// one with it does. An empty memory is refused before it gets here (Thetis
// sends "?").
std::deque<CwxSender::Element> CwxSender::elementsForMemory(const QString& text, int baseWpm,
                                                             int* characters, bool* loops) const
{
    std::deque<Element> out;
    int count = 0;
    bool npause = false;

    // the following inserts a slight delay between ptt and 1st element
    for (int nptt = kPttDelayMs / elementMs(baseWpm); nptt > 0; nptt--)
    {
        Element e;
        e.code = kElPtt;	// early ptt
        out.push_back(e);
    }

    for (const Piece& piece : parseMacroText(text, baseWpm)) {
        // convert keys to upper case
        const int cc = piece.ch.toUpper().unicode();		// fetch next character

        quint32 v = 0;					// clear element builder
        int ic = cc - ' ';			// convert character into
        if (ic < 0 || ic > 63) {
            continue;
        }
        ic &= 0x3f;				// 0-63 code

        if (ic == 2)	// set loop back
        {
            Element e;
            e.code = kElPause;		// stuff a pause code
            out.push_back(e);
            npause = true;
            break;					// all done
        }
        else if (ic == 3)		// a long dash
        {
            v = 0xffffff00 + 23;	// 23 marks
        }
        else if (ic == 4)		// a long space
        {
            v = 23;				// 23 spaces
        }
        else
        {
            v = m_mbits[static_cast<size_t>(ic)]; // fetch Morse bit pattern for the character
        }

        // top 24 of v are marks, bottom 5 have count

        quint32 n = v & 0x1f;		// isolate the element count
        bool first = true;
        while (n > 0)		// push n marks/spaces to the element fifo
        {
            Element e;
            e.code = (v & 0x80000000) > 0 ? kElKeyDown : kElKeyUp;
            e.speedDelta = static_cast<qint8>(piece.speedDelta);
            e.charIndex = piece.index;
            e.firstOfChar = first;
            first = false;
            out.push_back(e);
            v <<= 1;
            n--;
        }
        ++count;
    }

    if (npause == false) {
        Element e;
        // NereusSDR: the Rpt switch repeats a message without the marker.
        e.code = m_repeatOn ? kElPause : kElEnd;	// stuff an end command if no
        // pauses in the message
        out.push_back(e);
    }
    *characters = count;
    *loops = npause;
    return out;
}

// ---------------------------------------------------------------------------
// Requests and settings
// ---------------------------------------------------------------------------

QString CwxSender::gateWords() const
{
    RequestGate gate;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        gate = m_gate;
    }
    return gate ? gate() : QString();
}

void CwxSender::setRequestGate(RequestGate gate)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_gate = std::move(gate);
}

bool CwxSender::sendText(const QString& text)
{
    const QString words = gateWords();
    if (!words.isEmpty()) {
        emit refused(words);
        return false;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    int characters = 0;
    std::deque<Element> elements = elementsForText(text, m_speedWpm, &characters);
    if (elements.empty()) {
        lock.unlock();
        emit refused(QString::fromLatin1(kCwxNothingToSendText));
        return false;
    }
    if (m_keyHeld) {
        releaseKeyHeldLocked();
    }
    // Typed text after a repeating memory ends the repeat: the memory
    // finishes, then the text goes.
    m_repeatText.clear();
    m_repeatMarker = false;
    m_pause = 0;
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [](const Element& e) { return e.code == kElPause; }),
                  m_queue.end());
    m_queue.insert(m_queue.end(), elements.begin(), elements.end());
    m_queuedChars.fetch_add(characters, std::memory_order_acq_rel);
    m_messageQueued = true;
    setSendingLocked(true);
    startTickingLocked();
    return true;
}

bool CwxSender::sendMemory(int number)
{
    if (number < 1 || number > kMemoryCount) {
        emit refused(QString::fromLatin1(kCwxNoSuchMemoryText).arg(number));
        return false;
    }
    QString text;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        text = m_memories.value(number - 1);
    }
    const QString words = gateWords();
    if (!words.isEmpty()) {
        emit refused(words);
        return false;
    }
    if (text.trimmed().isEmpty()) {
        emit refused(QString::fromLatin1(kCwxMemoryEmptyText).arg(number));
        return false;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    int characters = 0;
    bool loops = false;
    std::deque<Element> elements = elementsForMemory(text, m_speedWpm, &characters, &loops);
    if (characters == 0) {
        lock.unlock();
        emit refused(QString::fromLatin1(kCwxNothingToSendText));
        return false;
    }
    // From Thetis cwx.cs:2354-2387 [v2.10.3.15] (queue_start): what is being
    // sent stops (quit), then the message loads.
    abortLocked();
    m_queue = std::move(elements);
    m_repeatText = text;
    m_repeatMarker = loops;
    m_queuedChars.store(characters, std::memory_order_release);
    m_messageQueued = true;
    setSendingLocked(true);
    startTickingLocked();
    return true;
}

void CwxSender::setMemories(const QStringList& eight)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    for (int i = 0; i < kMemoryCount; ++i) {
        m_memories[i] = eight.value(i);
    }
}

void CwxSender::stop()
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        abortLocked();
        if (m_messageQueued) {
            m_messageQueued = false;
            QMetaObject::invokeMethod(this, [this]() { emit emptied(); }, Qt::QueuedConnection);
        }
        setSendingLocked(false);
        m_wake.store(true, std::memory_order_release);
    }
    m_cv.notify_all();
}

// The Key: held while the operator holds it, dropped kKeyWatchdogMs after
// the last press with no release (the TCI rule, R-CW-32). From Thetis
// cwx.cs:1689-1719 [v2.10.3.15] (KeyAction): what is being sent stops, then
// PTT and the key go on together. Thetis's Key toggles and drops after 60 s.
void CwxSender::setKeyHeld(bool down)
{
    if (!down) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (m_keyHeld) {
            releaseKeyHeldLocked();
            m_wake.store(true, std::memory_order_release);
            m_cv.notify_all();
        }
        return;
    }
    const QString words = gateWords();
    if (!words.isEmpty()) {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            if (m_keyHeld) {
                releaseKeyHeldLocked();
            }
        }
        emit refused(words);
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_keyHeld) {
            if (m_ticking || !m_queue.empty()) {
                abortLocked();
                if (m_messageQueued) {
                    m_messageQueued = false;
                    QMetaObject::invokeMethod(this, [this]() { emit emptied(); },
                                              Qt::QueuedConnection);
                }
            }
            setKeyHeldStateLocked(true);
            m_keyPressedAt = m_clock();
            setPttLocked(true);
            setKeyLocked(true);
            setSendingLocked(true);
        }
        m_keyDeadline = m_clock() + std::chrono::milliseconds(kKeyWatchdogMs);
        m_wake.store(true, std::memory_order_release);
    }
    m_cv.notify_all();
}

// From Thetis TCIServer.cs:9050-9064 [v2.10.3.15] (scheduleKeyerReleaseLocked):
// the release falls `durationMs` after the press; when that time has passed
// the key is released at once. NereusSDR keeps it no later than the
// watchdog's deadline, so a long duration cannot hold the key past the 3 s
// rule (section 6). The sender's thread releases it at the deadline.
void CwxSender::releaseKeyAfter(int msAfterPress)
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_keyHeld) {
            return;
        }
        const TimePoint releaseAt = m_keyPressedAt + std::chrono::milliseconds(std::max(0, msAfterPress));
        if (releaseAt <= m_clock()) {
            releaseKeyHeldLocked();
        } else if (releaseAt < m_keyDeadline) {
            m_keyDeadline = releaseAt;
        }
        m_wake.store(true, std::memory_order_release);
    }
    m_cv.notify_all();
}

// R-CW-36: the serial straight-key line. Thetis writes NetworkIO.SetCWX from
// the pin change (SDRSerialPortII.cs:251, 256 [v2.10.3.15]); here it goes
// through the sender, the one writer of the CWX bit, and keys under the
// same break-in or MOX rule as everything the sender keys.
bool CwxSender::setHandKey(bool down)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!down) {
        if (m_handKey) {
            m_handKey = false;
            applyLineLocked();
        }
        return false;
    }
    if (m_handKey) {
        return true;
    }
    if (m_ticking || !m_queue.empty() || m_keyHeld) {
        return false;
    }
    m_handKey = true;
    applyLineLocked();
    return true;
}

void CwxSender::setKeyListener(KeyListener listener)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_keyListener = std::move(listener);
    if (m_keyListener) {
        m_keyListener(m_listenedKey);
    }
}

int CwxSender::queuedCharacters() const
{
    return std::max(0, m_queuedChars.load(std::memory_order_acquire));
}

void CwxSender::setKeyLine(std::shared_ptr<CwKeyLine> line)
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (line == m_line) {
            return;
        }
        // Everything stops and the old line is released before it is let go.
        abortLocked();
        if (m_messageQueued) {
            m_messageQueued = false;
            QMetaObject::invokeMethod(this, [this]() { emit emptied(); }, Qt::QueuedConnection);
        }
        setSendingLocked(false);
        m_line = std::move(line);
        m_lineKey = false;
        m_linePtt = false;
        m_wake.store(true, std::memory_order_release);
    }
    m_cv.notify_all();
}

void CwxSender::setHl2(bool hl2)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_hl2 = hl2;
    applyLineLocked();
}

void CwxSender::setSpeedWpm(int wpm)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_speedWpm = clampWpm(wpm);
}

void CwxSender::setRepeat(bool on, int delaySeconds)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_repeatOn = on;
    m_repeatDelaySeconds = std::clamp(delaySeconds, kMinRepeatDelaySeconds, kMaxRepeatDelaySeconds);
}

void CwxSender::setBreakInActive(bool on)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_breakIn = on;
    applyLineLocked();
}

void CwxSender::setMoxActive(bool on)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_mox = on;
    applyLineLocked();
}

} // namespace NereusSDR
