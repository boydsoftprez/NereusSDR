// NereusSDR-original strict Core LAN discovery codec.
#include "StationLanAnnouncement.h"

static_assert(4 + 1 + 1 + 2 + 95 + 1 + NereusSDR::kStationLanMaxCoreNameBytes + 1 + 1
                      + NereusSDR::kStationLanMaxRadioNameBytes + 17 + 1
                      + NereusSDR::kStationLanIdentityBytes + 1
                      + NereusSDR::kStationLanMaxLabelBytes + 1
                      // iPhone app Task 71: the device count.
                      + 1
                      // iPhone app plan Task 25: the radio state.
                      + 1
                  == NereusSDR::kStationLanMaxSchema2DatagramBytes,
              "the largest schema-2 announcement is 481 bytes");
static_assert(NereusSDR::kStationLanMaxSchema2DatagramBytes
                  <= NereusSDR::kStationLanMaxDatagramBytes,
              "a schema-2 announcement always fits a listener's datagram bound");

namespace NereusSDR {
namespace {

constexpr char kUnknownMac[] = "00:00:00:00:00:00";
constexpr int kFingerprintBytes = 95;
constexpr int kMacBytes = 17;

void setError(QString* error, const char* text)
{
    if (error) {
        *error = QString::fromLatin1(text);
    }
}

bool upperHex(char value)
{
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'F');
}

bool canonicalHexPairs(const QString& value, int expectedLength)
{
    if (value.size() != expectedLength) {
        return false;
    }
    for (int i = 0; i < value.size(); ++i) {
        const ushort unicode = value.at(i).unicode();
        if (unicode > 0x7f) {
            return false;
        }
        const char ascii = static_cast<char>(unicode);
        if ((i % 3 == 2 && ascii != ':') || (i % 3 != 2 && !upperHex(ascii))) {
            return false;
        }
    }
    return true;
}

bool validText(const QString& value, int minimumBytes, int maximumBytes)
{
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar character = value.at(i);
        if (character.isHighSurrogate()) {
            if (i + 1 >= value.size() || !value.at(i + 1).isLowSurrogate()) {
                return false;
            }
            const char32_t scalar = QChar::surrogateToUcs4(character, value.at(i + 1));
            if (QChar::category(scalar) == QChar::Other_Control) {
                return false;
            }
            ++i;
            continue;
        }
        if (character.isLowSurrogate() || character.category() == QChar::Other_Control) {
            return false;
        }
    }
    const QByteArray encoded = value.toUtf8();
    return encoded.size() >= minimumBytes && encoded.size() <= maximumBytes
        && encoded.isValidUtf8() && QString::fromUtf8(encoded) == value;
}

// Section 14 of the link document: a label is ASCII letters, digits, '/',
// '_' and '-' (StationLabel's alphabet), at most kStationLanMaxLabelBytes.
bool validLabel(const QString& value)
{
    if (value.size() > kStationLanMaxLabelBytes) {
        return false;
    }
    for (const QChar character : value) {
        const ushort unicode = character.unicode();
        const bool allowed = (unicode >= 'A' && unicode <= 'Z') || (unicode >= 'a' && unicode <= 'z')
            || (unicode >= '0' && unicode <= '9') || unicode == '/' || unicode == '_'
            || unicode == '-';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

bool validPairing(quint8 value)
{
    return value <= static_cast<quint8>(StationLanPairing::Code);
}

bool knownRadio(quint8 value)
{
    return value <= static_cast<quint8>(StationLanRadio::Waiting);
}

bool validate(const StationLanAnnouncement& value, QString* error)
{
    if (value.controlPort == 0) {
        setError(error, "Station LAN announcement has an invalid control port.");
        return false;
    }
    if (!canonicalHexPairs(value.fingerprint, kFingerprintBytes)) {
        setError(error, "Station LAN announcement has an invalid fingerprint.");
        return false;
    }
    if (!validText(value.coreName, 1, kStationLanMaxCoreNameBytes)) {
        setError(error, "Station LAN announcement has an invalid Core name.");
        return false;
    }
    if (!validText(value.radioName, value.radioConnected ? 1 : 0,
                   kStationLanMaxRadioNameBytes)) {
        setError(error, "Station LAN announcement has an invalid radio name.");
        return false;
    }
    if (!canonicalHexPairs(value.radioMac, kMacBytes)
        || (value.radioConnected && value.radioMac == QLatin1String(kUnknownMac))) {
        setError(error, "Station LAN announcement has an invalid radio MAC.");
        return false;
    }
    if (value.schema == kStationLanAnnouncementSchema1) {
        // Schema 1 has nowhere to put these, so a value holding them would
        // not come back from its own bytes.
        if (value.claimed || !value.identity.isEmpty() || !value.label.isEmpty()
            || value.pairing != StationLanPairing::Closed || value.devicesConnected
            || value.radio) {
            setError(error, "Station LAN announcement schema 1 cannot carry the Core's identity.");
            return false;
        }
        return true;
    }
    if (value.schema != kStationLanAnnouncementSchema2) {
        setError(error, "Station LAN announcement has an unsupported schema.");
        return false;
    }
    if (value.identity.size() != kStationLanIdentityBytes) {
        setError(error, "Station LAN announcement has an invalid identity.");
        return false;
    }
    if (!validLabel(value.label)) {
        setError(error, "Station LAN announcement has an invalid label.");
        return false;
    }
    if (!validPairing(static_cast<quint8>(value.pairing))) {
        setError(error, "Station LAN announcement has an invalid pairing state.");
        return false;
    }
    if (value.devicesConnected
        && (*value.devicesConnected < 0
            || *value.devicesConnected > kStationLanMaxDevicesConnected)) {
        setError(error, "Station LAN announcement has an invalid device count.");
        return false;
    }
    if (value.radio) {
        // It follows the device count on the wire, so it cannot come alone,
        // and it says the same as Radio connected.
        if (!value.devicesConnected || !knownRadio(static_cast<quint8>(*value.radio))
            || ((*value.radio == StationLanRadio::Connected) != value.radioConnected)) {
            setError(error, "Station LAN announcement has an invalid radio state.");
            return false;
        }
    }
    return true;
}

bool take(const QByteArray& bytes, int* offset, int count, QByteArray* result)
{
    if (count < 0 || *offset > bytes.size() - count) {
        return false;
    }
    *result = bytes.mid(*offset, count);
    *offset += count;
    return true;
}

bool takeByte(const QByteArray& bytes, int* offset, quint8* result)
{
    if (*offset >= bytes.size()) {
        return false;
    }
    *result = static_cast<quint8>(bytes.at((*offset)++));
    return true;
}

} // namespace

QString stationLanPairingName(StationLanPairing pairing)
{
    switch (pairing) {
    case StationLanPairing::Click:
        return QStringLiteral("click");
    case StationLanPairing::Code:
        return QStringLiteral("code");
    case StationLanPairing::Closed:
        break;
    }
    return QStringLiteral("closed");
}

std::optional<StationLanPairing> stationLanPairingFromName(const QString& name)
{
    for (const StationLanPairing pairing :
         {StationLanPairing::Closed, StationLanPairing::Click, StationLanPairing::Code}) {
        if (name == stationLanPairingName(pairing)) {
            return pairing;
        }
    }
    return std::nullopt;
}

QString stationLanRadioName(StationLanRadio radio)
{
    switch (radio) {
    case StationLanRadio::Connected:
        return QStringLiteral("connected");
    case StationLanRadio::Waiting:
        return QStringLiteral("waiting");
    case StationLanRadio::Offline:
        break;
    }
    return QStringLiteral("offline");
}

std::optional<StationLanRadio> stationLanRadioFromName(const QString& name)
{
    for (const StationLanRadio radio :
         {StationLanRadio::Offline, StationLanRadio::Connected, StationLanRadio::Waiting}) {
        if (name == stationLanRadioName(radio)) {
            return radio;
        }
    }
    return std::nullopt;
}

QByteArray encodeStationLanAnnouncement(const StationLanAnnouncement& value, QString* error)
{
    if (!validate(value, error)) {
        return {};
    }
    const QByteArray coreName = value.coreName.toUtf8();
    const QByteArray radioName = value.radioName.toUtf8();
    QByteArray out;
    out.reserve(kStationLanMaxSchema2DatagramBytes);
    out.append("NRSC", 4);
    out.append(static_cast<char>(value.schema));
    out.append(static_cast<char>(kStationLanWssControlService));
    out.append(static_cast<char>(value.controlPort >> 8));
    out.append(static_cast<char>(value.controlPort & 0xff));
    out.append(value.fingerprint.toLatin1());
    out.append(static_cast<char>(coreName.size()));
    out.append(coreName);
    out.append(value.radioConnected ? '\x01' : '\x00');
    out.append(static_cast<char>(radioName.size()));
    out.append(radioName);
    out.append(value.radioMac.toLatin1());
    if (value.schema == kStationLanAnnouncementSchema2) {
        const QByteArray label = value.label.toLatin1();
        out.append(value.claimed ? '\x01' : '\x00');
        out.append(value.identity);
        out.append(static_cast<char>(label.size()));
        out.append(label);
        out.append(static_cast<char>(value.pairing));
        // iPhone app Task 71 (ruling 10.4): appended after Pairing.
        if (value.devicesConnected) {
            out.append(static_cast<char>(*value.devicesConnected));
            // iPhone app plan Task 25 (R-IOS-16): appended after the count.
            if (value.radio) {
                out.append(static_cast<char>(*value.radio));
            }
        }
    }
    if (out.size() > kStationLanMaxDatagramBytes) {
        setError(error, "Station LAN announcement is too large.");
        return {};
    }
    if (error) {
        error->clear();
    }
    return out;
}

std::optional<StationLanAnnouncement> decodeStationLanAnnouncement(const QByteArray& bytes,
                                                                     QString* error)
{
    if (bytes.size() > kStationLanMaxDatagramBytes) {
        setError(error, "Station LAN announcement is too large.");
        return std::nullopt;
    }
    int offset = 0;
    QByteArray field;
    if (!take(bytes, &offset, 4, &field) || field != QByteArrayLiteral("NRSC")) {
        setError(error, "Station LAN announcement is malformed.");
        return std::nullopt;
    }
    quint8 schema = 0;
    if (!takeByte(bytes, &offset, &schema)) {
        setError(error, "Station LAN announcement is malformed.");
        return std::nullopt;
    }
    if (schema != kStationLanAnnouncementSchema1 && schema != kStationLanAnnouncementSchema2) {
        setError(error, "Station LAN announcement has an unsupported schema.");
        return std::nullopt;
    }
    quint8 service = 0;
    if (!takeByte(bytes, &offset, &service)) {
        setError(error, "Station LAN announcement is malformed.");
        return std::nullopt;
    }
    if (service != kStationLanWssControlService) {
        setError(error, "Station LAN announcement has an unsupported service.");
        return std::nullopt;
    }
    quint8 high = 0, low = 0, coreLength = 0, connected = 0, radioLength = 0;
    QByteArray fingerprint, coreName, radioName, radioMac;
    if (!takeByte(bytes, &offset, &high) || !takeByte(bytes, &offset, &low)
        || !take(bytes, &offset, kFingerprintBytes, &fingerprint)
        || !takeByte(bytes, &offset, &coreLength)
        || !take(bytes, &offset, coreLength, &coreName)
        || !takeByte(bytes, &offset, &connected)
        || !takeByte(bytes, &offset, &radioLength)
        || !take(bytes, &offset, radioLength, &radioName)
        || !take(bytes, &offset, kMacBytes, &radioMac)) {
        setError(error, "Station LAN announcement is malformed.");
        return std::nullopt;
    }
    quint8 claimed = 0, labelLength = 0, pairing = 0;
    QByteArray identity, label;
    if (schema == kStationLanAnnouncementSchema2
        && (!takeByte(bytes, &offset, &claimed)
            || !take(bytes, &offset, kStationLanIdentityBytes, &identity)
            || !takeByte(bytes, &offset, &labelLength)
            || !take(bytes, &offset, labelLength, &label)
            || !takeByte(bytes, &offset, &pairing))) {
        setError(error, "Station LAN announcement is malformed.");
        return std::nullopt;
    }
    // iPhone app Task 71 (ruling 10.4): the device count, when the Core
    // sent it; a datagram without it (an older Core) leaves it unknown.
    std::optional<int> devicesConnected;
    if (schema == kStationLanAnnouncementSchema2 && offset < bytes.size()) {
        quint8 count = 0;
        takeByte(bytes, &offset, &count);
        devicesConnected = count;
    }
    // iPhone app plan Task 25 (R-IOS-16): the radio state after the count,
    // when the Core sent it. A state this reader does not know (a later
    // Core's) reads as not known, so the Core is still listed.
    std::optional<StationLanRadio> radio;
    if (schema == kStationLanAnnouncementSchema2 && devicesConnected && offset < bytes.size()) {
        quint8 state = 0;
        takeByte(bytes, &offset, &state);
        if (knownRadio(state)) {
            radio = static_cast<StationLanRadio>(state);
        }
    }
    // Schema 2 extends by appending (link document section 14.1): a reader
    // ignores bytes after the fields it knows. Schema 1 stays exact.
    if (schema == kStationLanAnnouncementSchema1 && offset != bytes.size()) {
        setError(error, "Station LAN announcement is malformed.");
        return std::nullopt;
    }
    if (claimed > 1) {
        setError(error, "Station LAN announcement has an invalid claimed state.");
        return std::nullopt;
    }
    if (!validPairing(pairing)) {
        setError(error, "Station LAN announcement has an invalid pairing state.");
        return std::nullopt;
    }
    if (connected > 1) {
        setError(error, "Station LAN announcement has an invalid radio connection state.");
        return std::nullopt;
    }
    if (coreLength == 0 || coreLength > kStationLanMaxCoreNameBytes || !coreName.isValidUtf8()) {
        setError(error, "Station LAN announcement has an invalid Core name.");
        return std::nullopt;
    }
    if (radioLength > kStationLanMaxRadioNameBytes || !radioName.isValidUtf8()) {
        setError(error, "Station LAN announcement has an invalid radio name.");
        return std::nullopt;
    }
    StationLanAnnouncement value;
    value.controlPort = static_cast<quint16>((static_cast<quint16>(high) << 8) | low);
    value.fingerprint = QString::fromLatin1(fingerprint);
    value.coreName = QString::fromUtf8(coreName);
    value.radioName = QString::fromUtf8(radioName);
    value.radioMac = QString::fromLatin1(radioMac);
    value.radioConnected = connected == 1;
    value.schema = schema;
    if (schema == kStationLanAnnouncementSchema2) {
        value.claimed = claimed == 1;
        value.identity = identity;
        // Latin-1 keeps every byte as one character, so validate() refuses
        // anything outside the label alphabet rather than it vanishing here.
        value.label = QString::fromLatin1(label);
        value.pairing = static_cast<StationLanPairing>(pairing);
        value.devicesConnected = devicesConnected;
        value.radio = radio;
    }
    if (!validate(value, error)) {
        return std::nullopt;
    }
    if (error) {
        error->clear();
    }
    return value;
}

QString StationLanEndpoint::key() const
{
    const QChar separator(0x1f);
    return announcement.fingerprint + separator + address.toString() + separator
        + address.scopeId() + separator + QString::number(interfaceIndex) + separator
        + QString::number(announcement.controlPort);
}

QUrl StationLanEndpoint::url() const
{
    QString host = address.toString();
    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        if (!address.scopeId().isEmpty() && !host.contains(QLatin1Char('%'))) {
            host.append(QLatin1Char('%'));
            host.append(address.scopeId());
        }
        host.replace(QLatin1Char('%'), QStringLiteral("%25"));
        return QUrl(QStringLiteral("wss://[%1]:%2").arg(host).arg(announcement.controlPort),
                    QUrl::StrictMode);
    }
    return QUrl(QStringLiteral("wss://%1:%2").arg(host).arg(announcement.controlPort),
                QUrl::StrictMode);
}

} // namespace NereusSDR
