// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/DnsSdAdvertiserWindows.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 16 (D36): the Bonjour backend on Windows, through
// the DNS-SD functions Windows ships in dnsapi.dll (DnsServiceRegister and
// its companions, Windows 10 1903 and later). They are looked up at run
// time, so a Core on an older Windows starts and reports Bonjour as not
// available rather than failing to load. Nothing is added to the build.
//
// DnsServiceRegister completes on a thread of Windows' own; the completion
// is handed to the advertiser's thread before anything reads it. Changing
// the TXT record registers the service again (Windows has no call that
// replaces it in place). The request and its service instance live until
// the deregistration's completion (Part C fix wave, R2-M5; see
// Registration below). Not yet built or run on Windows.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R2-M4, R2-M5): the Avahi
//               subscription is checked and a daemon restart re-registers; the
//               Windows instance is freed only by the deregistration
//               completion. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

// The DNS-SD declarations in windns.h need Windows 10. This file is built
// without the precompiled header (CMakeLists.txt), so these come first.
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0A00
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
// NTDDI_WIN10_19H1 (1903), the first release with DnsServiceRegister,
// in case the header guards on the release rather than on Windows 10.
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000007
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>
#include <windns.h>

#include "DnsSdAdvertiser.h"

#include "core/LogCategories.h"

#include <QCoreApplication>
#include <QHostInfo>
#include <QMetaObject>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace NereusSDR {
namespace {

using ConstructInstanceFn = PDNS_SERVICE_INSTANCE(WINAPI*)(PCWSTR, PCWSTR, PIP4_ADDRESS,
                                                           PIP6_ADDRESS, WORD, WORD, WORD,
                                                           DWORD, PCWSTR*, PCWSTR*);
using RegisterFn = DWORD(WINAPI*)(PDNS_SERVICE_REGISTER_REQUEST, PDNS_SERVICE_CANCEL);
using DeRegisterFn = DWORD(WINAPI*)(PDNS_SERVICE_REGISTER_REQUEST, PDNS_SERVICE_CANCEL);
using FreeInstanceFn = VOID(WINAPI*)(PDNS_SERVICE_INSTANCE);

struct DnsApi {
    ConstructInstanceFn construct = nullptr;
    RegisterFn registerService = nullptr;
    DeRegisterFn deregisterService = nullptr;
    FreeInstanceFn freeInstance = nullptr;

    bool usable() const
    {
        return construct != nullptr && registerService != nullptr
            && deregisterService != nullptr && freeInstance != nullptr;
    }
};

const DnsApi& dnsApi()
{
    static const DnsApi api = [] {
        DnsApi result;
        HMODULE module = LoadLibraryExW(L"dnsapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (module == nullptr) {
            return result;
        }
        result.construct = reinterpret_cast<ConstructInstanceFn>(
            GetProcAddress(module, "DnsServiceConstructInstance"));
        result.registerService = reinterpret_cast<RegisterFn>(
            GetProcAddress(module, "DnsServiceRegister"));
        result.deregisterService = reinterpret_cast<DeRegisterFn>(
            GetProcAddress(module, "DnsServiceDeRegister"));
        result.freeInstance = reinterpret_cast<FreeInstanceFn>(
            GetProcAddress(module, "DnsServiceFreeInstance"));
        return result;
    }();
    return api;
}

// What Windows' completion thread may touch: a weak reference to the
// backend's completion handler, which only the application's thread
// follows (the backend lives there), so the backend can go away at any
// moment without a race.
struct CompletionHandler {
    std::function<void(DWORD status, quint64 generation)> handle;
};

// Part C fix wave (R2-M5): one registration, and everything Windows may
// still read while it is pending: the request and the instance it points
// at. Both completions carry it as their context (the request's
// pQueryContext). DnsServiceDeRegister "is asynchronous. The callback will
// be invoked when the deregistration is completed, with a copy of the
// DNS_SERVICE_INSTANCE structure that was passed to DnsServiceRegister"
// (Microsoft Learn, DnsServiceDeRegister, Remarks), so the instance and
// the request stay alive until that completion, which frees them; the
// registration's completion may come before it or not at all before the
// deregistration starts, so completions still owed are counted, with the
// deregistration's flag in the same atomic word, and whichever completion
// is the last one after the deregistration began frees it all.
struct Registration {
    static constexpr quint32 kDeregistering = 0x80000000u;
    static constexpr quint32 kCountMask = 0x7fffffffu;

    DNS_SERVICE_REGISTER_REQUEST request{};
    PDNS_SERVICE_INSTANCE instance = nullptr;
    std::weak_ptr<CompletionHandler> handler;
    quint64 generation = 0;
    // Completions still owed, and kDeregistering once
    // DnsServiceDeRegister was asked.
    std::atomic<quint32> state{0};
};

// Frees a registration Windows no longer reads: its instance (made by
// DnsServiceConstructInstance) and itself.
void release(Registration* registration)
{
    if (registration->instance != nullptr && dnsApi().freeInstance != nullptr) {
        dnsApi().freeInstance(registration->instance);
    }
    delete registration;
}

// One completion fewer is owed; the last one after the deregistration
// began releases the registration.
void completionArrived(Registration* registration)
{
    const quint32 before = registration->state.fetch_sub(1);
    if ((before & Registration::kCountMask) == 1 && (before & Registration::kDeregistering) != 0) {
        release(registration);
    }
}

VOID WINAPI onRegisterComplete(DWORD status, PVOID context, PDNS_SERVICE_INSTANCE instance)
{
    // The instance handed to a completion is a copy the caller frees
    // (Microsoft Learn, DNS_SERVICE_REGISTER_COMPLETE: "If not nullptr, then
    // you are responsible for freeing the data using
    // DnsServiceFreeInstance").
    if (instance != nullptr && dnsApi().freeInstance != nullptr) {
        dnsApi().freeInstance(instance);
    }
    auto* registration = static_cast<Registration*>(context);
    if (registration == nullptr) {
        return;
    }
    // Read before completionArrived(), which may free the registration.
    const std::weak_ptr<CompletionHandler> handler = registration->handler;
    const quint64 generation = registration->generation;
    completionArrived(registration);
    QCoreApplication* application = QCoreApplication::instance();
    if (application == nullptr) {
        return;
    }
    // The deregistration's own completion is posted too; its generation is
    // past, so the backend ignores it.
    QMetaObject::invokeMethod(
        application,
        [handler, generation, status]() {
            if (const std::shared_ptr<CompletionHandler> alive = handler.lock()) {
                alive->handle(status, generation);
            }
        },
        Qt::QueuedConnection);
}

class WindowsDnsSdBackend final : public DnsSdBackend {
public:
    WindowsDnsSdBackend()
        : m_handler(std::make_shared<CompletionHandler>())
    {
        m_handler->handle = [this](DWORD status, quint64 generation) {
            onCompleted(status, generation);
        };
    }
    ~WindowsDnsSdBackend() override { unregisterService(); }

    bool isAvailable() override { return dnsApi().usable(); }

    bool registerService(quint16 port, const DnsSdRecord& record,
                         const DnsSdTxtEntries& txt) override
    {
        unregisterService();
        const DnsApi& api = dnsApi();
        if (!api.usable() || record.interfaceIndex == kDnsSdThisComputerOnly) {
            return false;
        }
        const std::wstring service = (record.instanceName + QLatin1Char('.')
                                      + QString::fromLatin1(kDnsSdServiceType)
                                      + QStringLiteral(".local"))
                                         .toStdWString();
        const std::wstring host =
            (QHostInfo::localHostName() + QStringLiteral(".local")).toStdWString();
        std::vector<std::wstring> keys;
        std::vector<std::wstring> values;
        for (const auto& [key, value] : txt) {
            keys.push_back(QString::fromLatin1(key).toStdWString());
            values.push_back(QString::fromLatin1(value).toStdWString());
        }
        std::vector<PCWSTR> keyPointers;
        std::vector<PCWSTR> valuePointers;
        for (std::size_t i = 0; i < keys.size(); ++i) {
            keyPointers.push_back(keys[i].c_str());
            valuePointers.push_back(values[i].c_str());
        }
        auto registration = std::make_unique<Registration>();
        registration->instance =
            api.construct(service.c_str(), host.c_str(), nullptr, nullptr, port, 0, 0,
                          static_cast<DWORD>(keys.size()), keyPointers.data(),
                          valuePointers.data());
        if (registration->instance == nullptr) {
            return false;
        }
        registration->handler = m_handler;
        registration->generation = ++m_generation;
        DNS_SERVICE_REGISTER_REQUEST& request = registration->request;
        request.Version = DNS_QUERY_REQUEST_VERSION1;
        request.InterfaceIndex = record.interfaceIndex;
        request.pServiceInstance = registration->instance;
        request.pRegisterCompletionCallback = &onRegisterComplete;
        request.pQueryContext = registration.get();
        request.unicastEnabled = FALSE;
        // The registration's completion is owed from here.
        registration->state.store(1);
        if (api.registerService(&request, nullptr) != DNS_REQUEST_PENDING) {
            // Refused at once: no completion comes, and Windows holds
            // nothing.
            api.freeInstance(registration->instance);
            return false;
        }
        m_registration = registration.release(); // released by its last completion
        qCInfo(lcDiscovery).noquote() << "Bonjour: advertising" << record.instanceName << "as"
                                      << kDnsSdServiceType;
        return true;
    }

    bool updateTxt(const DnsSdRecord&, const DnsSdTxtEntries&) override { return false; }

    void unregisterService() override
    {
        ++m_generation;
        Registration* registration = m_registration;
        m_registration = nullptr;
        if (registration == nullptr) {
            return;
        }
        // The deregistration's completion is owed from here, and it (or the
        // registration's, if that comes later) frees the request and the
        // instance: never before, since Windows reads them until then.
        registration->state.fetch_add(1 + Registration::kDeregistering);
        const DnsApi& api = dnsApi();
        if (!api.usable()
            || api.deregisterService(&registration->request, nullptr) != DNS_REQUEST_PENDING) {
            // Refused at once: its completion will not come.
            completionArrived(registration);
        }
    }

private:
    void onCompleted(DWORD status, quint64 generation)
    {
        // A registration withdrawn since (unregisterService moves the
        // generation on) no longer speaks for this one.
        if (m_registration == nullptr || generation != m_generation
            || status == ERROR_SUCCESS) {
            return;
        }
        const QString reason =
            QStringLiteral("Windows could not publish the service (%1).").arg(status);
        unregisterService();
        if (failed) {
            failed(reason);
        }
    }

    std::shared_ptr<CompletionHandler> m_handler;
    // Not owned: released by its last completion (Registration).
    Registration* m_registration = nullptr;
    quint64 m_generation = 0;
};

} // namespace

std::unique_ptr<DnsSdBackend> createPlatformDnsSdBackend()
{
    return std::make_unique<WindowsDnsSdBackend>();
}

} // namespace NereusSDR
