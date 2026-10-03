// no-port-check: NereusSDR-original POSIX portability tests.
#include <QtTest>

#ifndef Q_OS_WIN
extern "C" {
#include "linux_port.h"
}

#include <array>
#include <chrono>
#include <cerrno>

class TestLinuxPortWait : public QObject {
    Q_OBJECT

private slots:
    void semaphoreHonorsInitialCount()
    {
        HANDLE semaphore = CreateSemaphoreW(0, 2, 2, nullptr);
        QVERIFY(semaphore != nullptr);
        QCOMPARE(WaitForSingleObject(static_cast<sem_t*>(semaphore), 0), static_cast<int>(WAIT_OBJECT_0));
        QCOMPARE(WaitForSingleObject(static_cast<sem_t*>(semaphore), 0), static_cast<int>(WAIT_OBJECT_0));
        QCOMPARE(WaitForSingleObject(static_cast<sem_t*>(semaphore), 0), static_cast<int>(WAIT_TIMEOUT));
        QCOMPARE(CloseHandle(semaphore), 0);
    }

    void waitAnyReturnsSignaledIndex()
    {
        std::array<HANDLE, 3> handles{
            CreateSemaphoreW(0, 0, 1, nullptr),
            CreateSemaphoreW(0, 0, 1, nullptr),
            CreateSemaphoreW(0, 0, 1, nullptr)};
        for (HANDLE handle : handles) {
            QVERIFY(handle != nullptr);
        }
        ReleaseSemaphore(static_cast<sem_t*>(handles[2]), 1, nullptr);
        QCOMPARE(WaitForMultipleObjects(static_cast<unsigned int>(handles.size()),
                                        handles.data(), FALSE, 100),
                 WAIT_OBJECT_0 + 2u);
        for (HANDLE handle : handles) {
            QCOMPARE(CloseHandle(handle), 0);
        }
    }

    void finiteWaitUsesDeadline()
    {
        HANDLE semaphore = CreateSemaphoreW(0, 0, 1, nullptr);
        QVERIFY(semaphore != nullptr);
        const auto start = std::chrono::steady_clock::now();
        QCOMPARE(WaitForSingleObject(static_cast<sem_t*>(semaphore), 20), static_cast<int>(WAIT_TIMEOUT));
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        QVERIFY(elapsed.count() >= 15);
        QVERIFY(elapsed.count() < 500);
        QCOMPARE(CloseHandle(semaphore), 0);
    }

    void readyHandlesRotate()
    {
        std::array<HANDLE, 2> handles{
            CreateSemaphoreW(0, 0, 1, nullptr),
            CreateSemaphoreW(0, 0, 1, nullptr)};
        bool sawFirst = false;
        bool sawSecond = false;
        for (int iteration = 0; iteration < 6; ++iteration) {
            ReleaseSemaphore(static_cast<sem_t*>(handles[0]), 1, nullptr);
            ReleaseSemaphore(static_cast<sem_t*>(handles[1]), 1, nullptr);
            const unsigned int selected = WaitForMultipleObjects(
                static_cast<unsigned int>(handles.size()), handles.data(), FALSE, 0);
            sawFirst |= selected == WAIT_OBJECT_0;
            sawSecond |= selected == WAIT_OBJECT_0 + 1u;
            const unsigned int other = selected == WAIT_OBJECT_0 ? 1u : 0u;
            QCOMPARE(WaitForSingleObject(static_cast<sem_t*>(handles[other]), 0),
                     static_cast<int>(WAIT_OBJECT_0));
        }
        QVERIFY(sawFirst);
        QVERIFY(sawSecond);
        for (HANDLE handle : handles) {
            QCOMPARE(CloseHandle(handle), 0);
        }
    }

    void waitAllIsExplicitlyUnsupported()
    {
        HANDLE semaphore = CreateSemaphoreW(0, 1, 1, nullptr);
        errno = 0;
        QCOMPARE(WaitForMultipleObjects(1, &semaphore, TRUE, 0), WAIT_FAILED);
        QCOMPARE(errno, ENOTSUP);
        QCOMPARE(CloseHandle(semaphore), 0);
    }
};

QTEST_APPLESS_MAIN(TestLinuxPortWait)
#include "tst_linux_port_wait.moc"

#else

class TestLinuxPortWait : public QObject {
    Q_OBJECT
private slots:
    void posixOnly() { QSKIP("POSIX portability test"); }
};

QTEST_APPLESS_MAIN(TestLinuxPortWait)
#include "tst_linux_port_wait.moc"

#endif
