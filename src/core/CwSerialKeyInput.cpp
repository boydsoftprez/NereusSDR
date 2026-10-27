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

#include "core/CwSerialKeyInput.h"

#include "core/CwKeyLine.h"
#include "core/LogCategories.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/platform/ThreadPlacement.h"

#include <QMetaObject>
#include <QtGlobal>

#include <chrono>
#include <condition_variable>
#include <utility>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <cerrno>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif
#if defined(Q_OS_LINUX)
#include <csignal>
#endif
#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace NereusSDR {

namespace {

void nameThisThread()
{
#if defined(Q_OS_MAC)
    pthread_setname_np("NereusCwSerialKey");
#elif defined(Q_OS_LINUX)
    // Linux allows 15 characters plus the terminator.
    pthread_setname_np(pthread_self(), "NereusCwSerKey");
#endif
}

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)

// ---------------------------------------------------------------------------
// POSIX: the port opened raw, without flow control, with DTR and RTS on.
// ---------------------------------------------------------------------------
class PosixLineSource : public CwSerialKeyInput::LineSource {
public:
    ~PosixLineSource() override { closePort(); }

    bool open(const QString& port, QString* error) override
    {
        closePort();
        m_stop.store(false, std::memory_order_release);
        // A bare name ("ttyUSB0", "cu.usbserial-1410") is under /dev.
        const QString path = port.startsWith(QLatin1Char('/'))
            ? port : QStringLiteral("/dev/") + port;
        const QByteArray native = path.toLocal8Bit();
        m_fd = ::open(native.constData(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
        if (m_fd < 0) {
            const int err = errno;
            qCWarning(lcConnection).noquote() << "CW serial key: cannot open" << path
                                              << "errno" << err;
            if (error != nullptr) {
                *error = QString::fromLatin1(kCwSerialPortOpenFailedText).arg(port);
            }
            return false;
        }
        // Exclusive, as QSerialPort opens CAT's ports: CAT cannot open the
        // port behind the reader (R-CW-04).
        ::ioctl(m_fd, TIOCEXCL);
        termios tio{};
        if (::tcgetattr(m_fd, &tio) == 0) {
            ::cfmakeraw(&tio);
            tio.c_cflag |= (CLOCAL | CREAD);
            // From Thetis SDRSerialPortII.cs:91 [v2.10.3.15]
            //   commPort.Handshake = Handshake.None; //[2.10.3.9]MW0LGE
            tio.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
            ::tcsetattr(m_fd, TCSANOW, &tio);
        }
        // From Thetis SDRSerialPortII.cs:83-84 [v2.10.3.15]
        //   commPort.RtsEnable = true; // hack for soft rock ptt
        //   commPort.DtrEnable = true; // set dtr off
        // A paddle or key wired to the port takes its supply from DTR or RTS.
        int out = TIOCM_DTR | TIOCM_RTS;
        ::ioctl(m_fd, TIOCMBIS, &out);
        if (!readLevels(&m_dsr, &m_cts)) {
            m_dsr = false;
            m_cts = false;
        }
        return true;
    }

    bool initialLevels(bool* dsr, bool* cts) override
    {
        *dsr = m_dsr;
        *cts = m_cts;
        return m_fd >= 0;
    }

protected:
    bool readLevels(bool* dsr, bool* cts) const
    {
        int bits = 0;
        if (m_fd < 0 || ::ioctl(m_fd, TIOCMGET, &bits) != 0) {
            return false;
        }
        *dsr = (bits & TIOCM_DSR) != 0;
        *cts = (bits & TIOCM_CTS) != 0;
        return true;
    }

    // A new level on either line: true with both levels set.
    bool takeChange(bool* dsr, bool* cts)
    {
        bool nowDsr = false;
        bool nowCts = false;
        if (!readLevels(&nowDsr, &nowCts)) {
            m_failed = true;
            return false;
        }
        if (nowDsr == m_dsr && nowCts == m_cts) {
            return false;
        }
        m_dsr = nowDsr;
        m_cts = nowCts;
        *dsr = nowDsr;
        *cts = nowCts;
        return true;
    }

    void closePort()
    {
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
    }

    int m_fd{-1};
    bool m_dsr{false};
    bool m_cts{false};
    bool m_failed{false};
    std::atomic<bool> m_stop{false};
};

#endif

#if defined(Q_OS_LINUX)

// ---------------------------------------------------------------------------
// Linux (D14): TIOCMIWAIT sleeps in the kernel until DSR or CTS changes. It
// has no time-out, so stop() wakes it with a signal whose handler does
// nothing (installed without SA_RESTART, so the ioctl returns EINTR), sent
// until the wait has seen the stop.
// ---------------------------------------------------------------------------
void wakeHandler(int) {}

int wakeSignal()
{
    static const int sig = [] {
        const int s = SIGRTMIN + 5;
        struct sigaction action {};
        action.sa_handler = &wakeHandler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        ::sigaction(s, &action, nullptr);
        return s;
    }();
    return sig;
}

class LinuxLineSource : public PosixLineSource {
public:
    LinuxLineSource() { wakeSignal(); }

    bool waitForChange(bool* dsr, bool* cts) override
    {
        m_failed = false;
        while (true) {
            m_waiter.store(pthread_self(), std::memory_order_release);
            m_inWait.store(true, std::memory_order_seq_cst);
            if (m_stop.load(std::memory_order_seq_cst)) {
                m_inWait.store(false, std::memory_order_release);
                closePort();
                return false;
            }
            const int r = ::ioctl(m_fd, TIOCMIWAIT, TIOCM_DSR | TIOCM_CTS);
            const int err = errno;
            m_inWait.store(false, std::memory_order_release);
            if (m_stop.load(std::memory_order_acquire)) {
                closePort();
                return false;
            }
            if (r != 0 && err != EINTR) {
                qCWarning(lcConnection) << "CW serial key: TIOCMIWAIT failed, errno" << err;
                closePort();
                return false;
            }
            if (takeChange(dsr, cts)) {
                return true;
            }
            if (m_failed) {
                closePort();
                return false;
            }
        }
    }

    void stop() override
    {
        m_stop.store(true, std::memory_order_seq_cst);
        // Up to 100 ms; the wait sees the stop before or after its ioctl.
        for (int i = 0; i < 100 && m_inWait.load(std::memory_order_seq_cst); ++i) {
            pthread_kill(m_waiter.load(std::memory_order_acquire), wakeSignal());
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

private:
    std::atomic<pthread_t> m_waiter{};
    std::atomic<bool> m_inWait{false};
};

#endif

#if defined(Q_OS_MAC)

// ---------------------------------------------------------------------------
// macOS (D14): no modem line wait exists, so the lines are read every 1 ms.
// The wait between reads is a timed condition wait, so the thread sleeps
// and stop() ends it at once.
// ---------------------------------------------------------------------------
class MacLineSource : public PosixLineSource {
public:
    // D14: the poll period.
    static constexpr std::chrono::milliseconds kPollPeriod{1};

    bool waitForChange(bool* dsr, bool* cts) override
    {
        m_failed = false;
        std::unique_lock<std::mutex> lock(m_mutex);
        while (true) {
            if (m_cv.wait_for(lock, kPollPeriod,
                              [this] { return m_stop.load(std::memory_order_acquire); })) {
                closePort();
                return false;
            }
            if (takeChange(dsr, cts)) {
                return true;
            }
            if (m_failed) {
                qCWarning(lcConnection) << "CW serial key: TIOCMGET failed, errno" << errno;
                closePort();
                return false;
            }
        }
    }

    void stop() override
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_stop.store(true, std::memory_order_release);
        }
        m_cv.notify_all();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

#endif

#if defined(Q_OS_WIN)

// ---------------------------------------------------------------------------
// Windows (D14): WaitCommEvent for EV_DSR | EV_CTS, overlapped, so stop()
// ends the wait through an event.
// ---------------------------------------------------------------------------
class WindowsLineSource : public CwSerialKeyInput::LineSource {
public:
    WindowsLineSource()
        : m_stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr))
        , m_ioEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr))
    {}
    ~WindowsLineSource() override
    {
        closePort();
        if (m_stopEvent != nullptr) {
            CloseHandle(m_stopEvent);
        }
        if (m_ioEvent != nullptr) {
            CloseHandle(m_ioEvent);
        }
    }

    bool open(const QString& port, QString* error) override
    {
        closePort();
        if (m_stopEvent != nullptr) {
            ResetEvent(m_stopEvent);
        }
        const QString path = port.startsWith(QLatin1String("\\\\.\\"))
            ? port : QStringLiteral("\\\\.\\") + port;
        m_port = CreateFileW(reinterpret_cast<const wchar_t*>(path.utf16()),
                             GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED, nullptr);
        if (m_port == INVALID_HANDLE_VALUE || m_stopEvent == nullptr || m_ioEvent == nullptr) {
            qCWarning(lcConnection).noquote() << "CW serial key: cannot open" << path
                                              << "error" << GetLastError();
            closePort();
            if (error != nullptr) {
                *error = QString::fromLatin1(kCwSerialPortOpenFailedText).arg(port);
            }
            return false;
        }
        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (GetCommState(m_port, &dcb)) {
            // From Thetis SDRSerialPortII.cs:83-84 [v2.10.3.15]
            //   commPort.RtsEnable = true; // hack for soft rock ptt
            //   commPort.DtrEnable = true; // set dtr off
            dcb.fDtrControl = DTR_CONTROL_ENABLE;
            dcb.fRtsControl = RTS_CONTROL_ENABLE;
            // From Thetis SDRSerialPortII.cs:91 [v2.10.3.15]
            //   commPort.Handshake = Handshake.None; //[2.10.3.9]MW0LGE
            dcb.fOutxCtsFlow = FALSE;
            dcb.fOutxDsrFlow = FALSE;
            dcb.fDsrSensitivity = FALSE;
            dcb.fOutX = FALSE;
            dcb.fInX = FALSE;
            SetCommState(m_port, &dcb);
        }
        SetCommMask(m_port, EV_DSR | EV_CTS);
        if (!readLevels(&m_dsr, &m_cts)) {
            m_dsr = false;
            m_cts = false;
        }
        return true;
    }

    bool initialLevels(bool* dsr, bool* cts) override
    {
        *dsr = m_dsr;
        *cts = m_cts;
        return m_port != INVALID_HANDLE_VALUE;
    }

    bool waitForChange(bool* dsr, bool* cts) override
    {
        while (true) {
            if (WaitForSingleObject(m_stopEvent, 0) == WAIT_OBJECT_0) {
                closePort();
                return false;
            }
            OVERLAPPED ov{};
            ov.hEvent = m_ioEvent;
            ResetEvent(m_ioEvent);
            DWORD mask = 0;
            if (!WaitCommEvent(m_port, &mask, &ov)) {
                if (GetLastError() != ERROR_IO_PENDING) {
                    qCWarning(lcConnection) << "CW serial key: WaitCommEvent failed, error"
                                            << GetLastError();
                    closePort();
                    return false;
                }
                const HANDLE handles[2] = {m_ioEvent, m_stopEvent};
                const DWORD woken = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                DWORD bytes = 0;
                if (woken != WAIT_OBJECT_0) {
                    CancelIo(m_port);
                    GetOverlappedResult(m_port, &ov, &bytes, TRUE);
                    closePort();
                    return false;
                }
                if (!GetOverlappedResult(m_port, &ov, &bytes, FALSE)) {
                    qCWarning(lcConnection) << "CW serial key: the line wait failed, error"
                                            << GetLastError();
                    closePort();
                    return false;
                }
            }
            bool nowDsr = false;
            bool nowCts = false;
            if (!readLevels(&nowDsr, &nowCts)) {
                closePort();
                return false;
            }
            if (nowDsr != m_dsr || nowCts != m_cts) {
                m_dsr = nowDsr;
                m_cts = nowCts;
                *dsr = nowDsr;
                *cts = nowCts;
                return true;
            }
        }
    }

    void stop() override
    {
        if (m_stopEvent != nullptr) {
            SetEvent(m_stopEvent);
        }
    }

private:
    bool readLevels(bool* dsr, bool* cts) const
    {
        DWORD status = 0;
        if (m_port == INVALID_HANDLE_VALUE || !GetCommModemStatus(m_port, &status)) {
            return false;
        }
        *dsr = (status & MS_DSR_ON) != 0;
        *cts = (status & MS_CTS_ON) != 0;
        return true;
    }

    void closePort()
    {
        if (m_port != INVALID_HANDLE_VALUE) {
            CloseHandle(m_port);
            m_port = INVALID_HANDLE_VALUE;
        }
    }

    HANDLE m_port{INVALID_HANDLE_VALUE};
    HANDLE m_stopEvent{nullptr};
    HANDLE m_ioEvent{nullptr};
    bool m_dsr{false};
    bool m_cts{false};
};

#endif

// A platform without a modem line reader: every open fails.
class NoLineSource : public CwSerialKeyInput::LineSource {
public:
    bool open(const QString& port, QString* error) override
    {
        if (error != nullptr) {
            *error = QString::fromLatin1(kCwSerialPortOpenFailedText).arg(port);
        }
        return false;
    }
    bool waitForChange(bool*, bool*) override { return false; }
    void stop() override {}
};

} // namespace

std::unique_ptr<CwSerialKeyInput::LineSource> CwSerialKeyInput::createPlatformLineSource()
{
#if defined(Q_OS_LINUX)
    return std::make_unique<LinuxLineSource>();
#elif defined(Q_OS_MAC)
    return std::make_unique<MacLineSource>();
#elif defined(Q_OS_WIN)
    return std::make_unique<WindowsLineSource>();
#else
    return std::make_unique<NoLineSource>();
#endif
}

CwSerialKeyInput::CwSerialKeyInput(std::unique_ptr<LineSource> source, QObject* parent)
    : QObject(parent)
    , m_source(source ? std::move(source) : std::make_unique<NoLineSource>())
{}

CwSerialKeyInput::~CwSerialKeyInput()
{
    stopReading();
    const std::lock_guard<std::mutex> lock(m_mutex);
    releaseAllLocked();
    m_line.reset();
    m_straightSink = nullptr;
}

bool CwSerialKeyInput::start(const QString& port, bool paddles, CwSerialLine keyLine,
                             CwSerialLine pttLine, QString* error)
{
    stopReading();
    if (!m_source->open(port, error)) {
        return false;
    }
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_paddles = paddles;
        m_keyLine = keyLine;
        m_pttLine = pttLine;
        bool dsr = false;
        bool cts = false;
        m_source->initialLevels(&dsr, &cts);
        // A line already down keys nothing until it goes up and down again.
        m_level = {dsr, cts};
        m_blocked = {dsr, cts};
        m_paddleDown.store(false, std::memory_order_release);
    }
    m_stopRequested.store(false, std::memory_order_release);
    m_reading.store(true, std::memory_order_release);
    m_thread = std::thread([this] { run(); });
    return true;
}

void CwSerialKeyInput::stopReading()
{
    if (m_thread.joinable()) {
        m_stopRequested.store(true, std::memory_order_release);
        m_source->stop();
        m_thread.join();
    }
    m_stopRequested.store(false, std::memory_order_release);
    m_reading.store(false, std::memory_order_release);
    const std::lock_guard<std::mutex> lock(m_mutex);
    releaseAllLocked();
    // Nothing is read now: no line counts as down.
    m_level = {false, false};
    m_blocked = {false, false};
    m_paddleDown.store(false, std::memory_order_release);
}

void CwSerialKeyInput::run()
{
    nameThisThread();
    // As the CWX sender: placed by ThreadPlacement where it manages thread
    // priority (nereusd), otherwise raised so a wake from the kernel's line
    // wait is not delayed behind ordinary work.
    const bool placed = ThreadPlacement::managesThreadPriority();
    AudioPriorityToken* prio = nullptr;
    if (placed) {
        ThreadPlacement::instance().registerCurrentThread(ThreadRole::CwSerialKey);
    } else {
#if defined(Q_OS_MAC)
        elevateLatencyCriticalThreadPriority(/*logSuccess=*/false);
#else
        prio = elevateAudioThreadPriority();
#endif
    }

    bool dsr = false;
    bool cts = false;
    while (m_source->waitForChange(&dsr, &cts)) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        onLevelsLocked(dsr, cts);
    }
    const bool lost = !m_stopRequested.load(std::memory_order_acquire);
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        releaseAllLocked();
        m_paddleDown.store(false, std::memory_order_release);
        if (lost) {
            QMetaObject::invokeMethod(
                this, [this]() { emit failed(QString::fromLatin1(kCwSerialPortLostText)); },
                Qt::QueuedConnection);
        }
    }
    m_reading.store(false, std::memory_order_release);

    if (placed) {
        ThreadPlacement::instance().deregisterCurrentThread();
    } else {
        leaveAudioThreadPriority(prio);
    }
}

void CwSerialKeyInput::onLevelsLocked(bool dsr, bool cts)
{
    // Thetis gets one pin change event per line, DSR's first when both move.
    if (dsr != m_level[kDsr]) {
        m_level[kDsr] = dsr;
        onEdgeLocked(kDsr, dsr);
    }
    if (cts != m_level[kCts]) {
        m_level[kCts] = cts;
        onEdgeLocked(kCts, cts);
    }
}

bool CwSerialKeyInput::lineIs(CwSerialLine chosen, Line line) const
{
    return (chosen == CwSerialLine::Dsr && line == kDsr)
        || (chosen == CwSerialLine::Cts && line == kCts);
}

// From Thetis SDRSerialPortII.cs:234-263 [v2.10.3.15], SerialPinChanged.
// Thetis's use_for_keyptt is a key line or a PTT line chosen here
// (setup.cs:8978-9026 [v2.10.3.15] sets the lines only with it), its
// ptt_on_dtr / key_on_dtr the DSR line and ptt_on_rts / key_on_rts the CTS
// line (SerialPinChange.DsrChanged and CtsChanged). The CAT PTT branch
// (265-269) is CatService's PTT input, not this reader.
void CwSerialKeyInput::onEdgeLocked(Line line, bool down)
{
    //refactored for MAX performance [2.10.3.9]MW0LGE
    // added else's to match switch's from old code

    const bool useForKeyPtt = m_keyLine != CwSerialLine::None || m_pttLine != CwSerialLine::None;
    if (useForKeyPtt) {
        if (lineIs(m_pttLine, line)) {
            setPttLocked(line, down);           // CWInput.KeyerPTT
        }
        if (lineIs(m_keyLine, line)) {
            setStraightLocked(line, down);      // NetworkIO.SetCWX
        }
    } else if (m_paddles) {
        setPaddleLocked(line, down);            // NetworkIO.SetCWDot / SetCWDash
        m_paddleDown.store(m_level[kDsr] || m_level[kCts], std::memory_order_release);
        QMetaObject::invokeMethod(this, [this]() { emit paddleActivity(); },
                                  Qt::QueuedConnection);
    }
    if (!down) {
        m_blocked[line] = false;
    }
    updateKeyingLocked();
}

void CwSerialKeyInput::setPttLocked(Line line, bool down)
{
    if (down) {
        if (m_blocked[line] || m_pttOn) {
            return;
        }
    } else if (!m_pttOn) {
        return;
    }
    m_pttOn = down;
    QMetaObject::invokeMethod(this, [this, down]() { emit pttLineChanged(down); },
                              Qt::QueuedConnection);
}

void CwSerialKeyInput::setStraightLocked(Line line, bool down)
{
    if (!down) {
        if (m_straightOn) {
            m_straightOn = false;
            if (m_straightSink) {
                m_straightSink(false);
            }
        }
        return;
    }
    if (m_blocked[line] || m_straightOn) {
        return;
    }
    if (!m_gateAllowed.load(std::memory_order_acquire) || !m_straightSink
        || !m_straightSink(true)) {
        m_blocked[line] = true;
        return;
    }
    m_straightOn = true;
}

void CwSerialKeyInput::setPaddleLocked(Line line, bool down)
{
    bool& on = line == kDsr ? m_dotOn : m_dashOn;
    if (!down) {
        if (on) {
            on = false;
            if (m_line) {
                if (line == kDsr) {
                    m_line->setDot(false);
                } else {
                    m_line->setDash(false);
                }
            }
        }
        return;
    }
    if (m_blocked[line] || on) {
        return;
    }
    if (!m_gateAllowed.load(std::memory_order_acquire) || !m_line) {
        m_blocked[line] = true;
        return;
    }
    on = true;
    if (line == kDsr) {
        m_line->setDot(true);
    } else {
        m_line->setDash(true);
    }
}

void CwSerialKeyInput::releaseKeyBitsLocked()
{
    if (m_dotOn) {
        m_dotOn = false;
        if (m_line) {
            m_line->setDot(false);
        }
    }
    if (m_dashOn) {
        m_dashOn = false;
        if (m_line) {
            m_line->setDash(false);
        }
    }
    if (m_straightOn) {
        m_straightOn = false;
        if (m_straightSink) {
            m_straightSink(false);
        }
    }
    // A line held down now keys nothing until it goes up and down again.
    for (int line = kDsr; line <= kCts; ++line) {
        if (m_level[line]) {
            m_blocked[line] = true;
        }
    }
    updateKeyingLocked();
}

void CwSerialKeyInput::releaseAllLocked()
{
    releaseKeyBitsLocked();
    if (m_pttOn) {
        m_pttOn = false;
        QMetaObject::invokeMethod(this, [this]() { emit pttLineChanged(false); },
                                  Qt::QueuedConnection);
    }
    updateKeyingLocked();
}

void CwSerialKeyInput::updateKeyingLocked()
{
    m_keying.store(m_dotOn || m_dashOn || m_straightOn || m_pttOn, std::memory_order_release);
}

void CwSerialKeyInput::setKeyLine(std::shared_ptr<CwKeyLine> line)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (line == m_line) {
        return;
    }
    // The old line is released before it is let go.
    releaseKeyBitsLocked();
    m_line = std::move(line);
}

void CwSerialKeyInput::setStraightKeySink(StraightKeySink sink)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    releaseKeyBitsLocked();
    m_straightSink = std::move(sink);
}

void CwSerialKeyInput::setGateAllowed(bool allowed)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_gateAllowed.store(allowed, std::memory_order_release);
    if (!allowed) {
        releaseKeyBitsLocked();
    }
}

void CwSerialKeyInput::releaseKeys()
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    releaseAllLocked();
}

} // namespace NereusSDR
