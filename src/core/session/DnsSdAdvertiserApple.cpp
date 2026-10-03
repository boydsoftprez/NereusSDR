// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/DnsSdAdvertiserApple.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 16 (D36): the Bonjour backend on macOS, through
// dns_sd.h, which is part of the system (libSystem and mDNSResponder); no
// library is added. DNSServiceRegister's reply arrives on the service's
// socket, which a QSocketNotifier watches on the advertiser's thread.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "DnsSdAdvertiser.h"

#include "core/LogCategories.h"

#include <QSocketNotifier>

#include <arpa/inet.h>
#include <dns_sd.h>

namespace NereusSDR {
namespace {

QByteArray txtBytes(const DnsSdTxtEntries& txt)
{
    TXTRecordRef ref;
    TXTRecordCreate(&ref, 0, nullptr);
    for (const auto& [key, value] : txt) {
        TXTRecordSetValue(&ref, key.constData(), static_cast<uint8_t>(value.size()),
                          value.constData());
    }
    const QByteArray bytes(static_cast<const char*>(TXTRecordGetBytesPtr(&ref)),
                           TXTRecordGetLength(&ref));
    TXTRecordDeallocate(&ref);
    return bytes;
}

uint32_t appleInterface(quint32 interfaceIndex)
{
    return interfaceIndex == kDnsSdThisComputerOnly ? kDNSServiceInterfaceIndexLocalOnly
                                                    : interfaceIndex;
}

class AppleDnsSdBackend final : public DnsSdBackend {
public:
    ~AppleDnsSdBackend() override { unregisterService(); }

    bool isAvailable() override { return true; }

    bool registerService(quint16 port, const DnsSdRecord& record,
                         const DnsSdTxtEntries& txt) override
    {
        unregisterService();
        const QByteArray name = record.instanceName.toUtf8();
        const QByteArray bytes = txtBytes(txt);
        DNSServiceRef ref = nullptr;
        const DNSServiceErrorType result = DNSServiceRegister(
            &ref, 0, appleInterface(record.interfaceIndex), name.constData(), kDnsSdServiceType,
            nullptr, nullptr, htons(port), static_cast<uint16_t>(bytes.size()),
            bytes.constData(), &AppleDnsSdBackend::onRegistered, this);
        if (result != kDNSServiceErr_NoError || ref == nullptr) {
            qCWarning(lcDiscovery) << "DNSServiceRegister refused:" << result;
            return false;
        }
        m_ref = ref;
        m_notifier = std::make_unique<QSocketNotifier>(DNSServiceRefSockFD(m_ref),
                                                       QSocketNotifier::Read);
        QObject::connect(m_notifier.get(), &QSocketNotifier::activated, m_notifier.get(),
                         [this]() { process(); });
        return true;
    }

    bool updateTxt(const DnsSdRecord&, const DnsSdTxtEntries& txt) override
    {
        if (m_ref == nullptr) {
            return false;
        }
        const QByteArray bytes = txtBytes(txt);
        // A null record reference names the service's own TXT record.
        return DNSServiceUpdateRecord(m_ref, nullptr, 0, static_cast<uint16_t>(bytes.size()),
                                      bytes.constData(), 0)
            == kDNSServiceErr_NoError;
    }

    void unregisterService() override
    {
        m_notifier.reset();
        if (m_ref != nullptr) {
            DNSServiceRefDeallocate(m_ref);
            m_ref = nullptr;
        }
    }

private:
    static void DNSSD_API onRegistered(DNSServiceRef, DNSServiceFlags,
                                       DNSServiceErrorType errorCode, const char* name,
                                       const char*, const char*, void* context)
    {
        auto* self = static_cast<AppleDnsSdBackend*>(context);
        if (errorCode == kDNSServiceErr_NoError) {
            qCInfo(lcDiscovery).noquote()
                << "Bonjour: advertising" << QString::fromUtf8(name) << "as" << kDnsSdServiceType;
            return;
        }
        self->m_failure = QStringLiteral("Bonjour registration failed (%1).").arg(errorCode);
    }

    void process()
    {
        if (m_ref == nullptr) {
            return;
        }
        const DNSServiceErrorType result = DNSServiceProcessResult(m_ref);
        QString failure = m_failure;
        m_failure.clear();
        if (result != kDNSServiceErr_NoError && failure.isEmpty()) {
            failure = QStringLiteral("Bonjour stopped answering (%1).").arg(result);
        }
        if (failure.isEmpty()) {
            return;
        }
        // This runs inside the notifier's own signal, so it is let go
        // rather than deleted here.
        m_notifier->setEnabled(false);
        m_notifier.release()->deleteLater();
        unregisterService();
        if (failed) {
            failed(failure);
        }
    }

    DNSServiceRef m_ref = nullptr;
    std::unique_ptr<QSocketNotifier> m_notifier;
    QString m_failure;
};

} // namespace

std::unique_ptr<DnsSdBackend> createPlatformDnsSdBackend()
{
    return std::make_unique<AppleDnsSdBackend>();
}

} // namespace NereusSDR
