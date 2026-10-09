// =================================================================
// src/core/audio/CaptureShm.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See CaptureShm.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 13 (R-AUD-17). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureShm.h"

#include "core/LogCategories.h"

#include <QtGlobal>

#if defined(Q_OS_WIN)
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace NereusSDR {

CaptureShmNames makeCaptureShmNames(qint64 pid, quint32 random)
{
    const QString stem = QStringLiteral("nrsc-%1-%2")
                             .arg(pid)
                             .arg(random, 8, 16, QLatin1Char('0'));
#if defined(Q_OS_WIN)
    const QString prefix = QStringLiteral("Local\\");
#else
    const QString prefix = QStringLiteral("/");
#endif
    return {prefix + stem + QLatin1Char('m'), prefix + stem + QLatin1Char('w')};
}

#if defined(Q_OS_WIN)

struct CaptureShmRegion::Impl {
    HANDLE mapping = nullptr;
    HANDLE wake = nullptr;
    void* data = nullptr;
    std::size_t size = 0;

    ~Impl()
    {
        if (data != nullptr) {
            UnmapViewOfFile(data);
        }
        if (mapping != nullptr) {
            CloseHandle(mapping);
        }
        if (wake != nullptr) {
            CloseHandle(wake);
        }
    }
};

std::unique_ptr<CaptureShmRegion> CaptureShmRegion::create(const CaptureShmNames& names,
                                                           std::size_t bytes)
{
    if (bytes == 0) {
        return nullptr;
    }
    auto impl = std::make_unique<Impl>();
    const std::wstring memoryName = names.memory.toStdWString();
    const std::wstring wakeName = names.wake.toStdWString();
    const auto wide = static_cast<unsigned long long>(bytes);
    impl->mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                       static_cast<DWORD>(wide >> 32),
                                       static_cast<DWORD>(wide & 0xFFFFFFFFull),
                                       memoryName.c_str());
    if (impl->mapping == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        qCWarning(lcAudio) << "CaptureShm: cannot create the mapping" << names.memory
                           << GetLastError();
        return nullptr;
    }
    impl->data = MapViewOfFile(impl->mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    if (impl->data == nullptr) {
        qCWarning(lcAudio) << "CaptureShm: cannot map" << names.memory << GetLastError();
        return nullptr;
    }
    impl->size = bytes;
    // Auto-reset: one SetEvent releases one wait.
    impl->wake = CreateEventW(nullptr, FALSE, FALSE, wakeName.c_str());
    if (impl->wake == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        qCWarning(lcAudio) << "CaptureShm: cannot create the wake event" << names.wake
                           << GetLastError();
        return nullptr;
    }
    return std::make_unique<CaptureShmRegion>(std::move(impl));
}

std::unique_ptr<CaptureShmRegion> CaptureShmRegion::attach(const CaptureShmNames& names,
                                                           std::size_t bytes)
{
    if (bytes == 0) {
        return nullptr;
    }
    auto impl = std::make_unique<Impl>();
    const std::wstring memoryName = names.memory.toStdWString();
    const std::wstring wakeName = names.wake.toStdWString();
    impl->mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, memoryName.c_str());
    if (impl->mapping == nullptr) {
        qCWarning(lcAudio) << "CaptureShm: cannot open the mapping" << names.memory
                           << GetLastError();
        return nullptr;
    }
    impl->data = MapViewOfFile(impl->mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (impl->data == nullptr) {
        qCWarning(lcAudio) << "CaptureShm: cannot map" << names.memory << GetLastError();
        return nullptr;
    }
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(impl->data, &info, sizeof(info)) == 0 || info.RegionSize < bytes) {
        qCWarning(lcAudio) << "CaptureShm: the mapping is smaller than" << bytes;
        return nullptr;
    }
    impl->size = bytes;
    impl->wake = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, wakeName.c_str());
    if (impl->wake == nullptr) {
        qCWarning(lcAudio) << "CaptureShm: cannot open the wake event" << names.wake
                           << GetLastError();
        return nullptr;
    }
    return std::make_unique<CaptureShmRegion>(std::move(impl));
}

void CaptureShmRegion::unlinkNames()
{
    // The objects go with their last handle.
}

void CaptureShmRegion::postWake()
{
    SetEvent(m_impl->wake);
}

bool CaptureShmRegion::waitWake()
{
    if (m_stop.load(std::memory_order_acquire)) {
        return false;
    }
    const DWORD result = WaitForSingleObject(m_impl->wake, INFINITE);
    if (result != WAIT_OBJECT_0) {
        qCWarning(lcAudio) << "CaptureShm: wait failed" << GetLastError();
        return false;
    }
    return !m_stop.load(std::memory_order_acquire);
}

#else

struct CaptureShmRegion::Impl {
    bool owner = false;
    bool unlinked = false;
    QByteArray memoryName;
    QByteArray wakeName;
    sem_t* wake = SEM_FAILED;
    void* data = MAP_FAILED;
    std::size_t size = 0;

    void unlink()
    {
        if (unlinked) {
            return;
        }
        unlinked = true;
        shm_unlink(memoryName.constData());
        sem_unlink(wakeName.constData());
    }

    ~Impl()
    {
        if (owner) {
            unlink();
        }
        if (data != MAP_FAILED) {
            munmap(data, size);
        }
        if (wake != SEM_FAILED) {
            sem_close(wake);
        }
    }
};

namespace {

bool namesFit(const CaptureShmNames& names)
{
    return names.memory.size() <= kCaptureShmNameMaxChars
        && names.wake.size() <= kCaptureShmNameMaxChars && names.memory.startsWith(QLatin1Char('/'))
        && names.wake.startsWith(QLatin1Char('/'));
}

} // namespace

std::unique_ptr<CaptureShmRegion> CaptureShmRegion::create(const CaptureShmNames& names,
                                                           std::size_t bytes)
{
    if (bytes == 0 || !namesFit(names)) {
        qCWarning(lcAudio) << "CaptureShm: bad names or size" << names.memory << names.wake << bytes;
        return nullptr;
    }
    auto impl = std::make_unique<Impl>();
    impl->memoryName = names.memory.toLatin1();
    impl->wakeName = names.wake.toLatin1();

    const int fd = shm_open(impl->memoryName.constData(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) {
        qCWarning(lcAudio) << "CaptureShm: shm_open failed" << names.memory << std::strerror(errno);
        return nullptr;
    }
    // From here the names are ours to remove, whatever happens next.
    impl->owner = true;
    if (ftruncate(fd, static_cast<off_t>(bytes)) != 0) {
        qCWarning(lcAudio) << "CaptureShm: ftruncate failed" << std::strerror(errno);
        close(fd);
        shm_unlink(impl->memoryName.constData());
        impl->unlinked = true;
        return nullptr;
    }
    impl->data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (impl->data == MAP_FAILED) {
        qCWarning(lcAudio) << "CaptureShm: mmap failed" << std::strerror(errno);
        shm_unlink(impl->memoryName.constData());
        impl->unlinked = true;
        return nullptr;
    }
    impl->size = bytes;
    impl->wake = sem_open(impl->wakeName.constData(), O_CREAT | O_EXCL, 0600, 0);
    if (impl->wake == SEM_FAILED) {
        qCWarning(lcAudio) << "CaptureShm: sem_open failed" << names.wake << std::strerror(errno);
        // Only the memory name is ours; the semaphore name may be another's.
        shm_unlink(impl->memoryName.constData());
        impl->unlinked = true;
        return nullptr;
    }
    return std::make_unique<CaptureShmRegion>(std::move(impl));
}

std::unique_ptr<CaptureShmRegion> CaptureShmRegion::attach(const CaptureShmNames& names,
                                                           std::size_t bytes)
{
    if (bytes == 0 || !namesFit(names)) {
        qCWarning(lcAudio) << "CaptureShm: bad names or size" << names.memory << names.wake << bytes;
        return nullptr;
    }
    auto impl = std::make_unique<Impl>();
    impl->memoryName = names.memory.toLatin1();
    impl->wakeName = names.wake.toLatin1();

    const int fd = shm_open(impl->memoryName.constData(), O_RDWR, 0600);
    if (fd < 0) {
        qCWarning(lcAudio) << "CaptureShm: shm_open failed" << names.memory << std::strerror(errno);
        return nullptr;
    }
    struct stat info {};
    if (fstat(fd, &info) != 0 || info.st_size < static_cast<off_t>(bytes)) {
        qCWarning(lcAudio) << "CaptureShm: the region is smaller than" << bytes;
        close(fd);
        return nullptr;
    }
    impl->data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (impl->data == MAP_FAILED) {
        qCWarning(lcAudio) << "CaptureShm: mmap failed" << std::strerror(errno);
        return nullptr;
    }
    impl->size = bytes;
    impl->wake = sem_open(impl->wakeName.constData(), 0);
    if (impl->wake == SEM_FAILED) {
        qCWarning(lcAudio) << "CaptureShm: sem_open failed" << names.wake << std::strerror(errno);
        return nullptr;
    }
    return std::make_unique<CaptureShmRegion>(std::move(impl));
}

void CaptureShmRegion::unlinkNames()
{
    m_impl->unlink();
}

void CaptureShmRegion::postWake()
{
    sem_post(m_impl->wake);
}

bool CaptureShmRegion::waitWake()
{
    if (m_stop.load(std::memory_order_acquire)) {
        return false;
    }
    while (sem_wait(m_impl->wake) != 0) {
        if (errno != EINTR) {
            qCWarning(lcAudio) << "CaptureShm: sem_wait failed" << std::strerror(errno);
            return false;
        }
    }
    return !m_stop.load(std::memory_order_acquire);
}

#endif

CaptureShmRegion::CaptureShmRegion(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}

CaptureShmRegion::~CaptureShmRegion() = default;

void* CaptureShmRegion::data() const
{
    return m_impl->data;
}

std::size_t CaptureShmRegion::size() const
{
    return m_impl->size;
}

void CaptureShmRegion::shutdownWake()
{
    m_stop.store(true, std::memory_order_release);
    postWake();
}

} // namespace NereusSDR
