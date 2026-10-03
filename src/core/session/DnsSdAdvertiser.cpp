// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/DnsSdAdvertiser.cpp  (NereusSDR)
// =================================================================
// See DnsSdAdvertiser.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): the sixth TXT entry,
//               `devices`. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25 (R-IOS-16): the seventh TXT entry,
//               `radio`. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "DnsSdAdvertiser.h"

#include "core/LogCategories.h"

#include <QNetworkInterface>

namespace NereusSDR {
namespace {

constexpr int kMaxTxtEntryBytes = 255;

void setError(QString* error, const QString& text)
{
    if (error) {
        *error = text;
    }
}

// Section 14.1's label rule, which the announcement's codec also applies:
// encoding a schema-2 announcement with this label and identity is the
// check, so the two can never disagree.
bool advertisable(const DnsSdRecord& record, QString* error)
{
    StationLanAnnouncement probe;
    probe.controlPort = 1;
    probe.fingerprint = QStringLiteral("00:").repeated(31) + QStringLiteral("00");
    probe.coreName = QStringLiteral("Core");
    probe.radioMac = QStringLiteral("00:00:00:00:00:00");
    probe.schema = kStationLanAnnouncementSchema2;
    probe.identity = record.identity;
    probe.label = record.label;
    probe.pairing = record.pairing;
    probe.devicesConnected = record.devicesConnected;
    QString codecError;
    if (encodeStationLanAnnouncement(probe, &codecError).isEmpty()) {
        setError(error, QStringLiteral("Bonjour record: %1").arg(codecError));
        return false;
    }
    const QByteArray instance = record.instanceName.toUtf8();
    if (instance.isEmpty() || instance.size() > kDnsSdMaxInstanceNameBytes
        || dnsSdInstanceName(record.instanceName) != record.instanceName) {
        setError(error, QStringLiteral("Bonjour record: the instance name is not usable."));
        return false;
    }
    return true;
}

class UnavailableDnsSdBackend final : public DnsSdBackend {
public:
    bool isAvailable() override { return false; }
    bool registerService(quint16, const DnsSdRecord&, const DnsSdTxtEntries&) override
    {
        return false;
    }
    bool updateTxt(const DnsSdRecord&, const DnsSdTxtEntries&) override { return false; }
    void unregisterService() override {}
};

} // namespace

#ifndef NEREUS_HAVE_DNSSD_BACKEND
std::unique_ptr<DnsSdBackend> createPlatformDnsSdBackend()
{
    return std::make_unique<UnavailableDnsSdBackend>();
}
#endif

DnsSdTxtEntries dnsSdTxtEntries(const DnsSdRecord& record, QString* error)
{
    if (!advertisable(record, error)) {
        return {};
    }
    const QByteArray id = record.identity
                              .toBase64(QByteArray::Base64UrlEncoding
                                        | QByteArray::OmitTrailingEquals)
                              .left(kDnsSdIdentityPrefixChars);
    DnsSdTxtEntries entries{
        {QByteArrayLiteral("v"), QByteArray(kDnsSdTxtVersion)},
        {QByteArrayLiteral("id"), id},
        {QByteArrayLiteral("claimed"), record.claimed ? QByteArrayLiteral("1")
                                                      : QByteArrayLiteral("0")},
        {QByteArrayLiteral("pair"), stationLanPairingName(record.pairing).toLatin1()},
        {QByteArrayLiteral("name"), record.label.toLatin1()},
        // iPhone app Task 71 (ruling 10.4): how many devices hold a place.
        {QByteArrayLiteral("devices"), QByteArray::number(record.devicesConnected)},
        // iPhone app plan Task 25 (R-IOS-16): the station's radio state.
        {QByteArrayLiteral("radio"), stationLanRadioName(record.radio).toLatin1()},
    };
    if (error) {
        error->clear();
    }
    return entries;
}

QByteArray encodeDnsSdTxtRecord(const DnsSdRecord& record, QString* error)
{
    const DnsSdTxtEntries entries = dnsSdTxtEntries(record, error);
    QByteArray out;
    for (const auto& [key, value] : entries) {
        const QByteArray entry = key + '=' + value;
        // The longest entry is name= and a 65-character label.
        Q_ASSERT(entry.size() <= kMaxTxtEntryBytes);
        out.append(static_cast<char>(entry.size()));
        out.append(entry);
    }
    return out;
}

std::optional<DnsSdTxtEntries> decodeDnsSdTxtRecord(const QByteArray& bytes, QString* error)
{
    DnsSdTxtEntries entries;
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const int length = static_cast<quint8>(bytes.at(offset++));
        if (length == 0 || offset + length > bytes.size()) {
            setError(error, QStringLiteral("Bonjour TXT record is malformed."));
            return std::nullopt;
        }
        const QByteArray entry = bytes.mid(offset, length);
        offset += length;
        const qsizetype equals = entry.indexOf('=');
        if (equals == 0) {
            setError(error, QStringLiteral("Bonjour TXT record has an entry with no key."));
            return std::nullopt;
        }
        entries.append(equals < 0 ? qMakePair(entry, QByteArray())
                                  : qMakePair(entry.left(equals), entry.mid(equals + 1)));
    }
    if (error) {
        error->clear();
    }
    return entries;
}

QString dnsSdInstanceName(const QString& displayName)
{
    QString result;
    int bytes = 0;
    for (const char32_t codepoint : displayName.toUcs4()) {
        if (QChar::category(codepoint) == QChar::Other_Control
            || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            continue;
        }
        const QString character = QString::fromUcs4(&codepoint, 1);
        const int size = static_cast<int>(character.toUtf8().size());
        if (bytes + size > kDnsSdMaxInstanceNameBytes) {
            break;
        }
        result += character;
        bytes += size;
    }
    result = result.trimmed();
    return result.isEmpty() ? QStringLiteral("NereusSDR Core") : result;
}

std::optional<quint32> dnsSdInterfaceForListener(const QHostAddress& listener)
{
    if (listener.isNull() || listener.isLoopback() || listener.isMulticast()
        || listener == QHostAddress::Broadcast) {
        return std::nullopt;
    }
    if (listener == QHostAddress::Any || listener == QHostAddress::AnyIPv4
        || listener == QHostAddress::AnyIPv6) {
        return kDnsSdAllInterfaces;
    }
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        if (interface.flags().testFlag(QNetworkInterface::IsLoopBack) || interface.index() <= 0) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
            QHostAddress address = entry.ip();
            address.setScopeId(QString());
            QHostAddress wanted = listener;
            wanted.setScopeId(QString());
            if (address == wanted) {
                return static_cast<quint32>(interface.index());
            }
        }
    }
    return std::nullopt;
}

DnsSdAdvertiser::DnsSdAdvertiser(QObject* parent)
    : DnsSdAdvertiser(createPlatformDnsSdBackend(), parent)
{
}

DnsSdAdvertiser::DnsSdAdvertiser(std::unique_ptr<DnsSdBackend> backend, QObject* parent)
    : QObject(parent)
    , m_backend(backend ? std::move(backend) : std::make_unique<UnavailableDnsSdBackend>())
{
    m_backend->failed = [this](const QString& reason) {
        qCWarning(lcDiscovery).noquote() << "Bonjour advertisement stopped:" << reason;
        m_active = false;
        m_port = 0;
        m_record = {};
        emit failed(reason);
    };
}

DnsSdAdvertiser::~DnsSdAdvertiser()
{
    stop();
    m_backend->failed = nullptr;
}

QString DnsSdAdvertiser::unavailableText()
{
    return QStringLiteral(
        "This Core cannot make itself known to iPhones and iPads on this network, so "
        "they will not find it by themselves. They can still reach it by its address.");
}

bool DnsSdAdvertiser::isAvailable() const
{
    return m_backend->isAvailable();
}

bool DnsSdAdvertiser::start(quint16 port, const DnsSdRecord& record)
{
    QString error;
    const DnsSdTxtEntries txt = dnsSdTxtEntries(record, &error);
    if (port == 0 || txt.isEmpty()) {
        qCWarning(lcDiscovery).noquote() << "Bonjour advertisement refused:"
                                         << (port == 0 ? QStringLiteral("no port") : error);
        stop();
        return false;
    }
    if (m_active && m_port == port && m_record.instanceName == record.instanceName
        && m_record.interfaceIndex == record.interfaceIndex) {
        // Already registered here: no need to ask the platform again.
        update(record);
        return m_active;
    }
    if (!m_backend->isAvailable()) {
        if (!m_unavailableLogged) {
            m_unavailableLogged = true;
            qCWarning(lcDiscovery).noquote() << unavailableText();
        }
        stop();
        return false;
    }
    stop();
    return registerNow(port, record, txt);
}

void DnsSdAdvertiser::update(const DnsSdRecord& record)
{
    if (!m_active || record == m_record) {
        return;
    }
    QString error;
    const DnsSdTxtEntries txt = dnsSdTxtEntries(record, &error);
    if (txt.isEmpty()) {
        qCWarning(lcDiscovery).noquote() << "Bonjour advertisement refused:" << error;
        stop();
        return;
    }
    if (record.instanceName == m_record.instanceName
        && record.interfaceIndex == m_record.interfaceIndex && m_backend->updateTxt(record, txt)) {
        m_record = record;
        return;
    }
    const quint16 port = m_port;
    stop();
    registerNow(port, record, txt);
}

void DnsSdAdvertiser::stop()
{
    if (m_active) {
        m_backend->unregisterService();
    }
    m_active = false;
    m_port = 0;
    m_record = {};
}

bool DnsSdAdvertiser::registerNow(quint16 port, const DnsSdRecord& record,
                                  const DnsSdTxtEntries& txt)
{
    if (!m_backend->registerService(port, record, txt)) {
        qCWarning(lcDiscovery) << "Bonjour advertisement could not be registered on port" << port;
        return false;
    }
    m_active = true;
    m_port = port;
    m_record = record;
    return true;
}

} // namespace NereusSDR
