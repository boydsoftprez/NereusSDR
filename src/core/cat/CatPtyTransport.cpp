// no-port-check: NereusSDR-original POSIX PTY mechanics; no upstream code port.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CatPtyTransport.h"
#include "core/LogCategories.h"
#include <QPointer>
#include <utility>
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif
namespace NereusSDR {
namespace {
// Nereus event-loop/output limits, independent of CAT wire and DSP constants.
constexpr int kPeerObservationMs = 20;
constexpr int kReadChunkBytes = 4096;
constexpr int kReadChunksPerActivation = 16;
constexpr qsizetype kMaximumOutputBytes = 64 * 1024;
constexpr qsizetype kMaximumPendingBytes = 256 * 1024;
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
QString systemError(const char* operation) { return QString::fromLatin1(operation) + ": " + QString::fromLocal8Bit(std::strerror(errno)); }
bool configureRaw(int descriptor, QString& error) {
    termios attributes{};
    if (::tcgetattr(descriptor, &attributes) != 0) { error = systemError("tcgetattr"); return false; }
    ::cfmakeraw(&attributes);
    if (::tcsetattr(descriptor, TCSANOW, &attributes) != 0) { error = systemError("tcsetattr"); return false; }
    termios accepted{};
    if (::tcgetattr(descriptor, &accepted) != 0) { error = systemError("tcgetattr verification"); return false; }
    if ((accepted.c_lflag & (ICANON | ECHO | ECHONL | ISIG | IEXTEN)) != 0 || (accepted.c_oflag & OPOST) != 0
        || accepted.c_iflag != attributes.c_iflag
        || (accepted.c_cflag & (CSIZE | PARENB)) != CS8
        || accepted.c_cc[VMIN] != attributes.c_cc[VMIN] || accepted.c_cc[VTIME] != attributes.c_cc[VTIME]) {
        error = "PTY driver did not accept raw terminal settings"; return false;
    }
    return true;
}

#endif
}
struct CatPtyTransport::Descriptor {
    int value{-1};
    explicit Descriptor(int descriptor) : value(descriptor) {}
    ~Descriptor() {
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        // Never retry close after EINTR: the descriptor may already have been reused.
        if (value >= 0 && ::close(value) != 0) { qCWarning(lcCat) << systemError("PTY close"); }
#endif
    }
};
CatPtyTransport::CatPtyTransport(QObject* parent) : QObject(parent) {
    m_peerTimer.setInterval(kPeerObservationMs);
    connect(&m_peerTimer, &QTimer::timeout, this, [this] { observePeer(); });
}
CatPtyTransport::~CatPtyTransport() { stop(); }
bool CatPtyTransport::isOpen() const { return m_master && m_master->value >= 0; }
bool CatPtyTransport::start(int channel, const CatEndpointConfig& config) {
    stop(); m_error.clear();
    if (channel < 1 || channel > 4) { fail("Invalid PTY channel"); return false; }
    if (config.ptyDialect != "Thetis") { fail("Rigctld PTY backend unavailable"); return false; }
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    auto master = std::make_unique<Descriptor>(::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC));
    if (master->value < 0) { fail(systemError("posix_openpt")); return false; }
    if (::grantpt(master->value) != 0) { fail(systemError("grantpt")); return false; }
    if (::unlockpt(master->value) != 0) { fail(systemError("unlockpt")); return false; }
    const char* name = ::ptsname(master->value);
    if (!name) { fail(systemError("ptsname")); return false; }
    const QString path = QString::fromLocal8Bit(name);
    // Open only our freshly allocated slave for setup, then close it. No anchor hides peer loss.
    {
        Descriptor slave(::open(path.toLocal8Bit().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC));
        if (slave.value < 0) { fail(systemError("PTY slave open")); return false; }
        QString error;
        if (!configureRaw(slave.value, error)) { fail(error); return false; }
    }
    m_master = std::move(master); m_path = path;
    // Deferred QObject disposal protects Qt's notifier stack during synchronous callback deletion.
    m_readNotifier = {std::make_unique<QSocketNotifier>(m_master->value, QSocketNotifier::Read).release(),
        [](QSocketNotifier* notifier) { notifier->deleteLater(); }};
    m_writeNotifier = {std::make_unique<QSocketNotifier>(m_master->value, QSocketNotifier::Write).release(),
        [](QSocketNotifier* notifier) { notifier->deleteLater(); }};
    m_readNotifier->setEnabled(false); m_writeNotifier->setEnabled(false);
    connect(m_readNotifier.get(), &QSocketNotifier::activated, this, [this] { readReady(); });
    connect(m_writeNotifier.get(), &QSocketNotifier::activated, this, [this] { drain(); });
    // PTY is one shared kernel stream, not a process/client identity. HUP absence is observed
    // on readiness or this bounded timer; a close+reopen entirely between observations has
    // no retained kernel history here and cannot promise a new session or stale-claim cleanup.
    // Closing the setup slave establishes HUP before the first real peer on both supported kernels.
    m_peerTimer.start();
    return true;
#else
    fail("Native PTY unavailable on this platform"); return false;
#endif
}
void CatPtyTransport::stop() {
    ++m_generation; m_peerTimer.stop(); m_peer = false; m_sessionId = 0; m_pending.clear(); m_path.clear();
    if (m_readNotifier) { m_readNotifier->setEnabled(false); m_readNotifier.reset(); }
    if (m_writeNotifier) { m_writeNotifier->setEnabled(false); m_writeNotifier.reset(); }
    m_master.reset();
}
void CatPtyTransport::fail(const QString& error) {
    stop(); m_error = error;
    qCWarning(lcCat) << "PTY CAT:" << error;
    emit failed(error);
}
bool CatPtyTransport::attachSession(quint64 id) {
    if (!id || !isOpen() || !m_peer || m_sessionId) { return false; }
    m_sessionId = id; return true;
}
void CatPtyTransport::closeSession(quint64 id) {
    if (id && id == m_sessionId) { stop(); }
}
void CatPtyTransport::losePeer() {
    if (!m_peer) { return; }
    m_peer = false; m_pending.clear();
    const quint64 id = std::exchange(m_sessionId, 0);
    m_readNotifier->setEnabled(false); m_writeNotifier->setEnabled(false);
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    // Drop both kernel queues as well as our tail before a later slave opener can inherit bytes.
    if (::tcflush(m_master->value, TCIOFLUSH) != 0) { fail(systemError("PTY flush after peer loss")); return; }
#if defined(Q_OS_LINUX)
    // Linux master tcflush leaves the linked slave's input queue intact. A
    // transient handle to our own slave purges that old outbound tail before
    // notifying the service. Close it now: it must never anchor a peer/session.
    {
        Descriptor slave(::open(m_path.toLocal8Bit().constData(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC));
        if (slave.value < 0) { fail(systemError("PTY slave open after peer loss")); return; }
        if (::tcflush(slave.value, TCIOFLUSH) != 0) { fail(systemError("PTY slave flush after peer loss")); return; }
    }
#endif
#endif
    emit peerClosed(id);
}
bool CatPtyTransport::observePeer() {
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    if (!isOpen()) { return false; }
    pollfd descriptor{m_master->value, POLLIN | POLLOUT, 0};
    const int result = ::poll(&descriptor, 1, 0);
    if (result < 0) { if (errno == EINTR) { return false; } fail(systemError("PTY poll")); return false; }
    if (descriptor.revents & POLLNVAL) { fail("PTY descriptor invalid"); return false; }
    // HUP is endpoint peer absence, not an endpoint creation failure. Never enable its read notifier.
    if (descriptor.revents & (POLLHUP | POLLERR)) { losePeer(); return false; }
    if (!m_peer) {
        // macOS resets termios at the last slave close. Its mode change can discard unread
        // initial slave output, so retain a bounded first burst before applying raw mode.
        // Nothing is parsed or replied to until raw mode and the new session are installed.
        QByteArray initial;
        bool drained = false;
        for (int chunk = 0; chunk <= kReadChunksPerActivation; ++chunk) {
            char buffer[kReadChunkBytes];
            const ssize_t count = ::read(m_master->value, buffer, sizeof(buffer));
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { drained = true; break; }
            if (count < 0 && errno == EINTR) { continue; }
            if (count == 0 || (count < 0 && errno == EIO)) { return false; }
            if (count < 0) { fail(systemError("PTY initial read")); return false; }
            if (initial.size() + count > kReadChunkBytes * kReadChunksPerActivation) {
                fail("PTY initial input limit exceeded"); return false;
            }
            initial.append(buffer, count);
        }
        if (!drained) { fail("PTY initial input budget exceeded"); return false; }
        QString error;
        if (!configureRaw(m_master->value, error)) { fail(error); return false; }
        m_peer = true;
        m_readNotifier->setEnabled(true);
        const QPointer<CatPtyTransport> self(this);
        const quint64 generation = m_generation;
        emit peerOpened();
        if (!self || generation != m_generation) { return false; }
        if (!initial.isEmpty()) { emit bytesReceived(initial); }
        if (!self || generation != m_generation) { return false; }
    }
    return m_peer;
#else
    return false;
#endif
}
void CatPtyTransport::readReady() {
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    const QPointer<CatPtyTransport> self(this);
    const quint64 generation = m_generation;
    if (!observePeer() || !self || generation != m_generation) { return; }
    for (int chunk = 0; chunk < kReadChunksPerActivation; ++chunk) {
        char buffer[kReadChunkBytes];
        const ssize_t count = ::read(m_master->value, buffer, sizeof(buffer));
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) { return; }
        if (count == 0 || (count < 0 && errno == EIO)) { losePeer(); return; }
        if (count < 0) { fail(systemError("PTY read")); return; }
        emit bytesReceived(QByteArray(buffer, count));
        if (!self || generation != m_generation || !m_peer) { return; }
    }
#endif
}
bool CatPtyTransport::writeBytes(quint64 id, const QByteArray& bytes) {
    const QPointer<CatPtyTransport> self(this);
    const quint64 generation = m_generation;
    if (!id || id != m_sessionId || !observePeer() || !self || generation != m_generation) { return false; }
    if (bytes.size() > kMaximumOutputBytes || m_pending.size() + bytes.size() > kMaximumPendingBytes) {
        fail("PTY output queue limit exceeded"); return false;
    }
    m_pending += bytes; drain();
    return self && generation == m_generation && m_peer;
}
void CatPtyTransport::drain() {
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    const QPointer<CatPtyTransport> self(this);
    const quint64 generation = m_generation;
    if (!observePeer() || !self || generation != m_generation || m_pending.isEmpty()) { return; }
    const ssize_t count = ::write(m_master->value, m_pending.constData(), static_cast<size_t>(m_pending.size()));
    if (count < 0 && errno == EIO) { losePeer(); return; }
    if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) { fail(systemError("PTY write")); return; }
    if (count > 0) { m_pending.remove(0, count); }
    // One nonblocking write per activation retains ordered tails without a retry loop.
    m_writeNotifier->setEnabled(!m_pending.isEmpty());
#endif
}
} // namespace NereusSDR
