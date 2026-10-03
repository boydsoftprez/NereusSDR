// SPDX-License-Identifier: GPL-2.0-or-later
// NereusSDR-original local/remote station DSP asset contract.

#include "DspAssetService.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/ModelPaths.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <utility>

namespace NereusSDR {

namespace {

constexpr auto kSelection0 = "DspAssets/NnrModel0";
constexpr auto kSelection1 = "DspAssets/NnrModel1";
constexpr auto kRevision = "DspAssets/SelectionRevision";
// R-R3-21: the one Core-wide NR3 model choice, and the once-only marker for
// carrying an older install's Nr3ModelPath file into the asset store.
constexpr auto kNr3Selection = "DspAssets/Nr3Model";
constexpr auto kNr3LegacyImported = "DspAssets/Nr3ModelPathImported";
constexpr auto kLegacyNr3ModelPath = "Nr3ModelPath";
constexpr int kMaxRecords = 128;
constexpr int kMaxPendingRequests = 128;
constexpr int kMaxImportsPerOwner = 2;
constexpr int kMaxImportsTotal = 4;

QString bundledId(int slot)
{
    return QStringLiteral("bundled:%1").arg(slot);
}

bool hasOnlyKeys(const QVariantMap& args, std::initializer_list<const char*> keys)
{
    if (args.size() != static_cast<qsizetype>(keys.size())) return false;
    for (const char* key : keys) {
        if (!args.contains(QString::fromLatin1(key))) return false;
    }
    return true;
}

bool exactString(const QVariant& value, QString* result)
{
    if (value.metaType().id() != QMetaType::QString) return false;
    if (result) *result = value.toString();
    return true;
}

bool exactBool(const QVariant& value, bool* result)
{
    if (value.metaType().id() != QMetaType::Bool) return false;
    if (result) *result = value.toBool();
    return true;
}

bool exactInteger(const QVariant& value, qint64* result)
{
    qint64 converted = 0;
    switch (value.metaType().id()) {
    case QMetaType::Int: converted = value.toInt(); break;
    case QMetaType::UInt: converted = value.toUInt(); break;
    case QMetaType::LongLong: converted = value.toLongLong(); break;
    case QMetaType::ULongLong: {
        const qulonglong input = value.toULongLong();
        if (input > static_cast<qulonglong>(std::numeric_limits<qint64>::max())) return false;
        converted = static_cast<qint64>(input);
        break;
    }
    default: return false;
    }
    if (result) *result = converted;
    return true;
}

qint64 kindLimit(DspAssetKind kind)
{
    return DspAssetValidation::sizeLimit(kind);
}

QString largeNr3Id() { return QString::fromLatin1(DspAssetService::kNr3BundledLargeId); }
QString smallNr3Id() { return QString::fromLatin1(DspAssetService::kNr3BundledSmallId); }

// Reads a model file with the size cap and runs the same trial load an
// imported model gets. Used for the bundled files, which the store does not
// hold.
// Fix wave I3 test seam: see DspAssetService::setBundledNr3ModelPathsForTest.
std::function<QString(const QString&)>& bundledNr3PathResolver()
{
    static std::function<QString(const QString&)> resolver;
    return resolver;
}

bool nr3FileUsable(const QString& path)
{
    if (path.isEmpty()) return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray bytes = file.read(DspAssetValidation::kMaxNr3ModelBytes + 1);
    return DspAssetValidation::validateNr3Model(bytes).accepted;
}

QString bundledNr3Name(const QString& id)
{
    return id == smallNr3Id() ? QObject::tr("the bundled small model")
                              : QObject::tr("the bundled large model");
}

QJsonObject recordJson(const DspAssetRecord& record)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), record.id);
    object.insert(QStringLiteral("kind"), static_cast<int>(record.kind));
    object.insert(QStringLiteral("hash"), record.hashHex);
    object.insert(QStringLiteral("size"), double(record.size));
    object.insert(QStringLiteral("format"), record.format);
    object.insert(QStringLiteral("version"), record.version);
    object.insert(QStringLiteral("compatibility"), record.compatibility);
    object.insert(QStringLiteral("numericEncoding"), record.numericEncoding);
    object.insert(QStringLiteral("label"), record.label);
    object.insert(QStringLiteral("radioIdentity"), record.radioIdentity);
    object.insert(QStringLiteral("valid"), record.valid);
    object.insert(QStringLiteral("validationError"), record.validationError);
    return object;
}

QString compactJson(const QJsonValue& value)
{
    if (value.isArray()) return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
}

} // namespace

struct DspAssetService::ActiveImport {
    QString owner;
    QString storeToken;
    DspAssetKind kind{DspAssetKind::NnrModel};
    qint64 expectedSize{0};
    qint64 received{0};
    QString expectedHash;
    QCryptographicHash hash{QCryptographicHash::Sha256};
};

DspAssetService::DspAssetService(AppSettings& settings, bool local, QObject* parent)
    : QObject(parent), m_settings(settings), m_local(local)
{
    m_selected = {bundledId(0), bundledId(1)};
    m_active = m_selected;
    m_lastResolved = m_active;
    m_nr3Selected = largeNr3Id();
    if (m_local) {
        m_store = std::make_unique<DspAssetStore>(QFileInfo(settings.filePath()).absolutePath());
        const QString saved0 = settings.value(QString::fromLatin1(kSelection0), bundledId(0)).toString();
        const QString saved1 = settings.value(QString::fromLatin1(kSelection1), bundledId(1)).toString();
        if (!saved0.isEmpty()) m_selected[0] = saved0;
        if (!saved1.isEmpty()) m_selected[1] = saved1;
        bool revisionOk = false;
        const qulonglong savedRevision = settings.value(QString::fromLatin1(kRevision),
                                                         QStringLiteral("1"))
                                                .toString().toULongLong(&revisionOk);
        m_revision = revisionOk && savedRevision > 0
                         ? static_cast<quint32>(std::min<qulonglong>(savedRevision,
                               std::numeric_limits<quint32>::max()))
                         : 1;
        settings.setValue(QString::fromLatin1(kRevision), QString::number(m_revision));
        importLegacyNr3ModelPath();
        const QString savedNr3 = settings.value(QString::fromLatin1(kNr3Selection),
                                                largeNr3Id()).toString();
        if (!savedNr3.isEmpty()) m_nr3Selected = savedNr3;
        // Status only; nothing is loaded until the Core applies it.
        resolveNr3ModelPath();
    }
    refreshSelectionStatus();
}

DspAssetService::~DspAssetService()
{
    resetSession();
}

bool DspAssetService::nnrModelSelectionPending() const
{
    return m_local ? m_selected != m_active : m_remotePending;
}

void DspAssetService::setRadioIdentity(const QString& mac)
{
    if (!m_local) return;
    m_radioIdentity = AppSettings::normalizedRadioMac(mac);
}

DspAssetServiceResult DspAssetService::reject(const QString& reason) const
{
    return {false, reason, {}};
}

DspAssetServiceResult DspAssetService::rejectDetail(const QString& detail,
                                                    const QString& plain) const
{
    if (DspAssetValidation::isOperatorMessage(detail)) {
        return reject(detail);
    }
    qCWarning(lcDsp).noquote() << "Model or correction file request refused:" << detail;
    return reject(plain);
}

DspAssetServiceResult DspAssetService::execute(const QByteArray& verb,
                                                const QVariantMap& args,
                                                const QString& owner)
{
    if (!m_local || !m_store) {
        return reject(QStringLiteral("The Core could not read this request."));
    }
    if (owner.isEmpty()) {
        return reject(QStringLiteral("The Core could not read this request."));
    }

    if (verb == "dspAssets.list") {
        if (!args.isEmpty()) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        QJsonArray assets;
        const QList<DspAssetRecord> records = m_store->assets();
        const qsizetype recordCount = std::min(records.size(), qsizetype(kMaxRecords));
        for (qsizetype i = 0; i < recordCount; ++i) {
            assets.append(recordJson(records.at(i)));
        }
        QVariantMap values;
        values.insert(QStringLiteral("assets"), compactJson(assets));
        values.insert(QStringLiteral("selection0"), m_selected[0]);
        values.insert(QStringLiteral("selection1"), m_selected[1]);
        values.insert(QStringLiteral("revision"), m_revision);
        values.insert(QStringLiteral("pending"), nnrModelSelectionPending());
        values.insert(QStringLiteral("status"), m_status);
        return {true, {}, values};
    }

    if (verb == "dspAssets.beginImport") {
        if (!hasOnlyKeys(args, {"kind", "label", "size", "hash", "radioIdentity"})) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        qint64 kindValue = -1;
        qint64 size = 0;
        QString label, hash, radioIdentity;
        DspAssetKind kind = DspAssetKind::NnrModel;
        if (!exactInteger(args.value(QStringLiteral("kind")), &kindValue)
            || !dspAssetKindFromInt(kindValue, &kind)
            || (kind == DspAssetKind::Nr3Model && !nr3ModelsSupported())
            || !exactString(args.value(QStringLiteral("label")), &label)
            || label.size() > 128
            || !exactInteger(args.value(QStringLiteral("size")), &size)
            || !exactString(args.value(QStringLiteral("hash")), &hash)
            || !exactString(args.value(QStringLiteral("radioIdentity")), &radioIdentity)) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        if (size <= 0 || size > kindLimit(kind)) {
            return reject(QStringLiteral("The file is too large for this kind of model or correction."));
        }
        static const QRegularExpression hex64(QStringLiteral("^[0-9A-Fa-f]{64}$"));
        if (!hex64.match(hash).hasMatch()) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        const QString normalizedRadio = AppSettings::normalizedRadioMac(radioIdentity);
        if (kind == DspAssetKind::NnrModel && !radioIdentity.isEmpty()) {
            return reject(QStringLiteral("An NNR model cannot be tied to one radio."));
        }
        if (kind == DspAssetKind::Nr3Model && !radioIdentity.isEmpty()) {
            return reject(QStringLiteral("NR3 models belong to the Core, not to one radio."));
        }
        if (kind == DspAssetKind::Ps3Correction
            && (normalizedRadio.isEmpty() || m_radioIdentity.isEmpty()
                || normalizedRadio != m_radioIdentity)) {
            return reject(QStringLiteral("This PureSignal correction was made for a different radio."));
        }
        if (m_imports.size() >= kMaxImportsTotal) {
            return reject(QStringLiteral("The Core is already importing as many files as it can. Try again when one finishes."));
        }
        int owned = 0;
        for (const auto& transfer : std::as_const(m_imports)) {
            if (transfer->owner == owner) {
                ++owned;
            }
        }
        if (owned >= kMaxImportsPerOwner) {
            return reject(QStringLiteral("This app is already importing as many files as it can. Try again when one finishes."));
        }
        QString error;
        const QString storeToken = m_store->beginImport(kind, label, normalizedRadio, &error);
        if (storeToken.isEmpty()) {
            return rejectDetail(error, QStringLiteral("The Core could not start storing this file."));
        }
        auto transfer = std::make_shared<ActiveImport>();
        transfer->owner = owner;
        transfer->storeToken = storeToken;
        transfer->kind = kind;
        transfer->expectedSize = size;
        transfer->expectedHash = hash.toLower();
        m_imports.insert(storeToken, transfer);
        return {true, {}, {{QStringLiteral("transferId"), storeToken}}};
    }

    if (verb == "dspAssets.chunk") {
        if (!hasOnlyKeys(args, {"transferId", "offset", "data"})) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        QString transferId, encoded;
        qint64 offset = -1;
        if (!exactString(args.value(QStringLiteral("transferId")), &transferId)
            || transferId.size() > 128
            || !exactInteger(args.value(QStringLiteral("offset")), &offset) || offset < 0
            || !exactString(args.value(QStringLiteral("data")), &encoded)
            || encoded.size() > 4 * ((DspAssetStore::kTransferChunkBytes + 2) / 3)) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        const auto transfer = m_imports.value(transferId);
        if (!transfer || transfer->owner != owner) {
            return reject(QStringLiteral("The import was lost on the Core. Start it again."));
        }
        if (offset != transfer->received) {
            return reject(QStringLiteral("The import was interrupted. Start it again."));
        }
        const QByteArray ascii = encoded.toLatin1();
        if (QString::fromLatin1(ascii) != encoded) {
            return reject(QStringLiteral("The import was interrupted. Start it again."));
        }
        const auto decodedResult = QByteArray::fromBase64Encoding(
            ascii, QByteArray::AbortOnBase64DecodingErrors);
        if (!decodedResult || decodedResult.decoded.size() > DspAssetStore::kTransferChunkBytes
            || decodedResult.decoded.toBase64() != ascii) {
            return reject(QStringLiteral("The import was interrupted. Start it again."));
        }
        if (decodedResult.decoded.size() > transfer->expectedSize - transfer->received) {
            m_store->cancelImport(transfer->storeToken);
            m_imports.remove(transferId);
            return reject(QStringLiteral("The import was interrupted. Start it again."));
        }
        QString error;
        if (!m_store->appendImport(transfer->storeToken, decodedResult.decoded, &error)) {
            m_imports.remove(transferId);
            return rejectDetail(error, QStringLiteral("The Core could not store this file. Start the import again."));
        }
        transfer->hash.addData(decodedResult.decoded);
        transfer->received += decodedResult.decoded.size();
        return {true, {}, {{QStringLiteral("offset"), transfer->received}}};
    }

    if (verb == "dspAssets.finishImport") {
        if (!hasOnlyKeys(args, {"transferId"})) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        QString transferId;
        if (!exactString(args.value(QStringLiteral("transferId")), &transferId)
            || transferId.size() > 128) {
            return reject(QStringLiteral("The import was interrupted. Start it again."));
        }
        const auto transfer = m_imports.value(transferId);
        if (!transfer || transfer->owner != owner) {
            return reject(QStringLiteral("The import was lost on the Core. Start it again."));
        }
        m_imports.remove(transferId);
        if (transfer->received != transfer->expectedSize
            || QString::fromLatin1(transfer->hash.result().toHex()) != transfer->expectedHash) {
            m_store->cancelImport(transfer->storeToken);
            return reject(QStringLiteral("The file did not reach the Core intact. Start the import again."));
        }
        const DspAssetImportResult imported = m_store->finishImport(transfer->storeToken);
        if (!imported.accepted) {
            return rejectDetail(imported.error,
                                QStringLiteral("This file is not a model or correction the Core can use."));
        }
        QVariantMap values;
        values.insert(QStringLiteral("id"), imported.record.id);
        values.insert(QStringLiteral("kind"), static_cast<int>(imported.record.kind));
        values.insert(QStringLiteral("hash"), imported.record.hashHex);
        values.insert(QStringLiteral("size"), imported.record.size);
        values.insert(QStringLiteral("format"), imported.record.format);
        values.insert(QStringLiteral("version"), imported.record.version);
        values.insert(QStringLiteral("compatibility"), imported.record.compatibility);
        values.insert(QStringLiteral("numericEncoding"), imported.record.numericEncoding);
        values.insert(QStringLiteral("label"), imported.record.label);
        values.insert(QStringLiteral("radioIdentity"), imported.record.radioIdentity);
        return {true, {}, values};
    }

    if (verb == "dspAssets.cancelImport") {
        if (!hasOnlyKeys(args, {"transferId"})) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        QString transferId;
        if (!exactString(args.value(QStringLiteral("transferId")), &transferId)
            || transferId.size() > 128) {
            return reject(QStringLiteral("The import was interrupted. Start it again."));
        }
        const auto transfer = m_imports.value(transferId);
        if (!transfer || transfer->owner != owner) {
            return reject(QStringLiteral("The import was lost on the Core. Start it again."));
        }
        m_store->cancelImport(transfer->storeToken);
        m_imports.remove(transferId);
        return {true, {}, {}};
    }

    if (verb == "dspAssets.export") {
        if (!hasOnlyKeys(args, {"id", "offset"})) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        QString id;
        qint64 offset = -1;
        if (!exactString(args.value(QStringLiteral("id")), &id) || id.size() > 128
            || !exactInteger(args.value(QStringLiteral("offset")), &offset) || offset < 0) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        DspAssetRecord record;
        bool found = false;
        for (const auto& candidate : m_store->assets()) {
            if (candidate.id == id) { record = candidate; found = true; break; }
        }
        if (!found) {
            return reject(QStringLiteral("That file is no longer on the Core."));
        }
        QString error;
        const QString path = m_store->resolvePath(id, record.kind,
            record.kind == DspAssetKind::Ps3Correction ? m_radioIdentity : QString(), &error);
        if (path.isEmpty()) {
            return rejectDetail(error, QStringLiteral("The Core could not read that file."));
        }
        if (offset > record.size) {
            return reject(QStringLiteral("The export was interrupted. Start it again."));
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || !file.seek(offset)) {
            return reject(QStringLiteral("The Core could not read that file."));
        }
        const QByteArray chunk = file.read(DspAssetStore::kTransferChunkBytes);
        QVariantMap values;
        values.insert(QStringLiteral("data"), QString::fromLatin1(chunk.toBase64()));
        values.insert(QStringLiteral("offset"), offset);
        values.insert(QStringLiteral("size"), record.size);
        values.insert(QStringLiteral("hash"), record.hashHex);
        values.insert(QStringLiteral("eof"), offset + chunk.size() >= record.size);
        return {true, {}, values};
    }

    if (verb == "dspAssets.selectNnrModel") {
        if (!hasOnlyKeys(args, {"slot", "id"})) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        qint64 slot = -1;
        QString id;
        if (!exactInteger(args.value(QStringLiteral("slot")), &slot) || slot < 0 || slot > 1
            || !exactString(args.value(QStringLiteral("id")), &id) || id.size() > 128) {
            return reject(QStringLiteral("The Core could not read this model choice."));
        }
        QString error;
        if (!setSelection(static_cast<int>(slot), id, &error)) {
            return reject(error);
        }
        return {true, {}, {{QStringLiteral("revision"), m_revision},
                           {QStringLiteral("status"), m_status}}};
    }

    if (verb == "dspAssets.selectNr3Model") {
        if (!hasOnlyKeys(args, {"id"})) {
            return reject(QStringLiteral("The Core could not read this request."));
        }
        QString id;
        if (!exactString(args.value(QStringLiteral("id")), &id) || id.size() > 128) {
            return reject(QStringLiteral("The Core could not read this model choice."));
        }
        QString error;
        if (!setNr3Selection(id, &error)) {
            return reject(error);
        }
        return {true, {}, {{QStringLiteral("id"), m_nr3Selected},
                           {QStringLiteral("status"), m_nr3Status}}};
    }

    return reject(QStringLiteral("The Core does not know this request."));
}

quint32 DspAssetService::allocateRequestId()
{
    for (int tries = 0; tries <= kMaxPendingRequests; ++tries) {
        quint32 candidate = m_nextRequestId++;
        if (candidate == 0) candidate = m_nextRequestId++;
        if (!m_pendingRequests.contains(candidate)) return candidate;
    }
    return 0;
}

quint32 DspAssetService::request(const QByteArray& verb, const QVariantMap& args)
{
    if (m_pendingRequests.size() >= kMaxPendingRequests) return 0;
    if (!m_local) {
        if (!m_remoteRequest) return 0;
        const quint32 id = m_remoteRequest(verb, args);
        if (id == 0 || m_pendingRequests.contains(id)) return 0;
        m_pendingRequests.insert(id, {verb});
        return id;
    }
    const quint32 id = allocateRequestId();
    if (id == 0) return 0;
    m_pendingRequests.insert(id, {verb});
    QTimer::singleShot(0, this, [this, id, verb, args] {
        const auto it = m_pendingRequests.find(id);
        if (it == m_pendingRequests.end() || it->verb != verb) return;
        m_pendingRequests.erase(it);
        const DspAssetServiceResult result = execute(verb, args, QStringLiteral("local"));
        emit requestCompleted(id, result.accepted, result.reason, result.values);
    });
    return id;
}

void DspAssetService::setRemoteRequestHandler(RemoteRequestHandler handler)
{
    if (!m_local) m_remoteRequest = std::move(handler);
}

bool DspAssetService::receiveRemoteResult(quint32 id, const QByteArray& verb,
                                          bool accepted, const QString& reason,
                                          const QVariantMap& values)
{
    if (m_local) return false;
    const auto it = m_pendingRequests.find(id);
    if (it == m_pendingRequests.end() || it->verb != verb) return false;
    m_pendingRequests.erase(it);
    emit requestCompleted(id, accepted, reason, values);
    return true;
}

void DspAssetService::resetSession()
{
    if (m_store) {
        for (const auto& transfer : std::as_const(m_imports))
            m_store->cancelImport(transfer->storeToken);
    }
    m_imports.clear();
    m_pendingRequests.clear();
    // Follow-up item 2 (R-R3-21): a window's copy of whether the Core can
    // run NR3, and why, belongs to that Core's session. The next Core says
    // again; an older one never says, and NR3 is then not refused here.
    if (!m_local) {
        setNr3Runnable(true);
        setNr3Status({});
        // R-R3-49: DFNR's availability the same way.
        if (!m_dfnrRunnable || !m_dfnrStatus.isEmpty()) {
            m_dfnrRunnable = true;
            m_dfnrStatus.clear();
            emit dfnrAvailabilityChanged();
        }
        // And MNR's.
        if (!m_mnrRunnable || !m_mnrStatus.isEmpty()) {
            m_mnrRunnable = true;
            m_mnrStatus.clear();
            emit mnrAvailabilityChanged();
        }
    }
}

void DspAssetService::cancelOwner(const QString& owner)
{
    if (!m_store) return;
    QList<QString> cancelled;
    for (auto it = m_imports.cbegin(); it != m_imports.cend(); ++it) {
        if (it.value()->owner == owner) {
            m_store->cancelImport(it.value()->storeToken);
            cancelled.append(it.key());
        }
    }
    for (const QString& token : cancelled) m_imports.remove(token);
}

void DspAssetService::refreshSelectionStatus()
{
    QStringList problems;
    if (m_local && m_store) {
        for (int slot = 0; slot < 2; ++slot) {
            if (m_selected[slot] == bundledId(slot)) {
                continue;
            }
            QString error;
            if (m_store->resolvePath(m_selected[slot], DspAssetKind::NnrModel, {}, &error).isEmpty()) {
                problems.append(tr("An added NNR model is missing or damaged, so the built-in one is used."));
            }
        }
    }
    problems.removeDuplicates();
    m_status = problems.isEmpty() ? tr("NNR model selections are available.")
                                  : problems.join(QLatin1Char(' '));
}

bool DspAssetService::setSelection(int slot, const QString& id, QString* reason)
{
    if (id != bundledId(slot)) {
        QString error;
        if (!m_store || m_store->resolvePath(id, DspAssetKind::NnrModel, {}, &error).isEmpty()) {
            if (!error.isEmpty()) {
                qCWarning(lcDsp).noquote() << "NNR model choice refused:" << error;
            }
            if (reason) {
                *reason = QStringLiteral("The chosen NNR model is not on the Core.");
            }
            return false;
        }
    }
    if (m_selected[slot] == id) {
        if (reason) {
            reason->clear();
        }
        return true;
    }
    m_selected[slot] = id;
    ++m_revision;
    if (m_revision == 0) {
        ++m_revision;
    }
    m_settings.setValue(slot == 0 ? QString::fromLatin1(kSelection0)
                                  : QString::fromLatin1(kSelection1), id);
    m_settings.setValue(QString::fromLatin1(kRevision), QString::number(m_revision));
    refreshSelectionStatus();
    emit selectionChanged();
    emit configurationChanged();
    if (reason) {
        reason->clear();
    }
    return true;
}

std::array<QString, 2> DspAssetService::resolveNnrModelPaths(QString* reason)
{
    std::array<QString, 2> paths;
    std::array<QString, 2> resolved{bundledId(0), bundledId(1)};
    QStringList problems;
    if (m_local && m_store) {
        for (int slot = 0; slot < 2; ++slot) {
            if (m_selected[slot] == bundledId(slot)) continue;
            QString error;
            const QString path = m_store->resolvePath(m_selected[slot], DspAssetKind::NnrModel,
                                                      {}, &error);
            if (path.isEmpty()) {
                problems.append(tr("An added NNR model is missing or damaged, so the built-in one is used."));
            } else {
                paths[slot] = path;
                resolved[slot] = m_selected[slot];
            }
        }
    }
    m_lastResolved = resolved;
    problems.removeDuplicates();
    const QString newStatus = problems.isEmpty() ? tr("NNR model selections are available.")
                                                  : problems.join(QLatin1Char(' '));
    if (m_status != newStatus) {
        m_status = newStatus;
        emit selectionChanged();
    }
    if (reason) *reason = problems.join(QLatin1Char(' '));
    return paths;
}

void DspAssetService::markNnrModelsApplied()
{
    resolveNnrModelPaths();
    if (m_active == m_lastResolved) return;
    m_active = m_lastResolved;
    emit selectionChanged();
}

bool DspAssetService::isBundledNr3Id(const QString& id)
{
    return id == largeNr3Id() || id == smallNr3Id();
}

QString DspAssetService::bundledNr3ModelPath(const QString& id)
{
    if (const auto& resolver = bundledNr3PathResolver()) return resolver(id);
    if (id == largeNr3Id()) return ModelPaths::rnnoiseDefaultLargeBin();
    if (id == smallNr3Id()) return ModelPaths::rnnoiseDefaultSmallBin();
    return {};
}

bool DspAssetService::nr3ModelsSupported() const
{
    if (!m_local) return m_remoteNr3Supported;
#ifdef HAVE_WDSP
    return m_store && m_store->isValid();
#else
    return false;
#endif
}

void DspAssetService::setRemoteNr3ModelsSupported(bool supported)
{
    if (m_local || m_remoteNr3Supported == supported) return;
    m_remoteNr3Supported = supported;
    emit nr3SelectionChanged();
}

void DspAssetService::setNr3ModelLoader(Nr3ModelLoader loader)
{
    if (m_local) m_nr3Loader = std::move(loader);
}

void DspAssetService::setBundledNr3ModelPathsForTest(
    std::function<QString(const QString&)> resolver)
{
    bundledNr3PathResolver() = std::move(resolver);
}

void DspAssetService::setNr3Runnable(bool runnable)
{
    if (m_nr3Runnable == runnable) return;
    m_nr3Runnable = runnable;
    emit nr3SelectionChanged();
}

void DspAssetService::setDfnrAvailability(bool runnable, const QString& status)
{
    if (!m_local) return;
    const QString text = runnable ? QString() : status;
    if (m_dfnrRunnable == runnable && m_dfnrStatus == text) return;
    m_dfnrRunnable = runnable;
    m_dfnrStatus = text;
    emit dfnrAvailabilityChanged();
}

void DspAssetService::setMnrAvailability(bool runnable, const QString& status)
{
    if (!m_local) return;
    const QString text = runnable ? QString() : status;
    if (m_mnrRunnable == runnable && m_mnrStatus == text) return;
    m_mnrRunnable = runnable;
    m_mnrStatus = text;
    emit mnrAvailabilityChanged();
}

void DspAssetService::setNr3Status(const QString& status)
{
    if (m_nr3Status == status) return;
    m_nr3Status = status;
    emit nr3SelectionChanged();
}

QString DspAssetService::resolveNr3ModelPath(QString* resolvedId, QString* reason)
{
    if (resolvedId) resolvedId->clear();
    if (reason) reason->clear();
    if (!m_local) {
        if (reason) *reason = tr("Only the Core loads NR3 models.");
        return {};
    }

    QString problem;
    if (!isBundledNr3Id(m_nr3Selected)) {
        QString error;
        const QString path = m_store
            ? m_store->resolvePath(m_nr3Selected, DspAssetKind::Nr3Model, {}, &error)
            : QString();
        if (!path.isEmpty()) {
            if (resolvedId) *resolvedId = m_nr3Selected;
            const QString label = m_store->label(m_nr3Selected);
            setNr3Status(label.isEmpty() ? tr("Using an added NR3 model.")
                                         : tr("Using the NR3 model \"%1\".").arg(label));
            setNr3Runnable(true);
            return path;
        }
        problem = tr("The chosen NR3 model is missing or damaged.");
    }

    // The bundled large model is the default; the small one stands in when
    // the large file is absent or damaged, and the other way round.
    const QStringList order = m_nr3Selected == smallNr3Id()
        ? QStringList{smallNr3Id(), largeNr3Id()}
        : QStringList{largeNr3Id(), smallNr3Id()};
    for (const QString& id : order) {
        const QString path = bundledNr3ModelPath(id);
        if (!nr3FileUsable(path)) continue;
        if (id != m_nr3Selected && problem.isEmpty()
            && isBundledNr3Id(m_nr3Selected)) {
            problem = m_nr3Selected == smallNr3Id()
                ? tr("The bundled small model is missing from this Core.")
                : tr("The bundled large model is missing from this Core.");
        }
        if (resolvedId) *resolvedId = id;
        const QString using_ = tr("Using %1.").arg(bundledNr3Name(id));
        setNr3Status(problem.isEmpty() ? using_ : problem + QLatin1Char(' ') + using_);
        setNr3Runnable(true);
        if (reason) *reason = problem;
        return path;
    }

    const QString none = tr("No NR3 model file was found on this Core, so NR3 cannot run.");
    setNr3Status(none);
    setNr3Runnable(false);
    if (reason) *reason = none;
    return {};
}

bool DspAssetService::applyNr3Model(QString* reason)
{
    QString id;
    const QString path = resolveNr3ModelPath(&id, reason);
    // Never hand the loader "" or a file that failed the trial load: WDSP's
    // RNNRloadModel would then leave NR3 with no model at all.
    if (path.isEmpty()) return false;
    if (m_nr3Loader) m_nr3Loader(path);
    if (m_nr3Active != id) {
        m_nr3Active = id;
        emit nr3SelectionChanged();
    }
    return true;
}

bool DspAssetService::setNr3Selection(const QString& id, QString* reason)
{
    if (!nr3ModelsSupported()) {
        if (reason) *reason = tr("This Core cannot change the NR3 model.");
        return false;
    }
    if (isBundledNr3Id(id)) {
        if (!nr3FileUsable(bundledNr3ModelPath(id))) {
            if (reason) *reason = tr("That bundled NR3 model is not installed on this Core.");
            return false;
        }
    } else {
        QString error;
        if (!m_store || m_store->resolvePath(id, DspAssetKind::Nr3Model, {}, &error).isEmpty()) {
            if (reason) *reason = tr("That NR3 model is missing or damaged on this Core.");
            return false;
        }
    }
    if (reason) reason->clear();
    if (m_nr3Selected == id && m_nr3Active == id) return true;
    const bool changed = m_nr3Selected != id;
    m_nr3Selected = id;
    if (changed) {
        m_settings.setValue(QString::fromLatin1(kNr3Selection), id);
    }
    // Live: rnnr.c's RNNRloadModel swaps the model under every NR3 instance.
    applyNr3Model();
    if (changed) {
        emit nr3SelectionChanged();
        emit configurationChanged();
    }
    return true;
}

void DspAssetService::importLegacyNr3ModelPath()
{
    if (!m_local || !m_store || !m_store->isValid()) return;
    const QString legacy = m_settings.value(QString::fromLatin1(kLegacyNr3ModelPath)).toString();
    if (legacy.isEmpty()
        || m_settings.value(QString::fromLatin1(kNr3LegacyImported)).toString()
               == QStringLiteral("True")) {
        return;
    }
    // Once only, whatever the outcome: a missing or damaged old file must not
    // be retried at every start.
    m_settings.setValue(QString::fromLatin1(kNr3LegacyImported), QStringLiteral("True"));
    if (m_settings.contains(QString::fromLatin1(kNr3Selection))) return;

    const QString canonical = QFileInfo(legacy).canonicalFilePath();
    for (const QString& id : {largeNr3Id(), smallNr3Id()}) {
        const QString bundled = QFileInfo(bundledNr3ModelPath(id)).canonicalFilePath();
        if (!canonical.isEmpty() && canonical == bundled) {
            m_settings.setValue(QString::fromLatin1(kNr3Selection), id);
            return;
        }
    }
    QFile file(legacy);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDsp) << "NR3: the older NR3 model file could not be read:" << legacy;
        return;
    }
    const QByteArray bytes = file.read(DspAssetValidation::kMaxNr3ModelBytes + 1);
    const DspAssetImportResult imported = m_store->importBytes(
        DspAssetKind::Nr3Model, QFileInfo(legacy).completeBaseName().left(128), bytes);
    if (!imported.accepted) {
        qCWarning(lcDsp) << "NR3: the older NR3 model file was not imported:" << imported.error;
        return;
    }
    m_settings.setValue(QString::fromLatin1(kNr3Selection), imported.record.id);
    qCInfo(lcDsp) << "NR3: imported the older NR3 model file as" << imported.record.id;
}

bool DspAssetService::applyRemoteProperty(const QByteArray& name, const QVariant& value)
{
    if (m_local) return false;
    if (name == "nr3Runnable") {
        bool runnable = true;
        if (!exactBool(value, &runnable)) return false;
        setNr3Runnable(runnable);
        return true;
    }
    if (name == "dfnrRunnable") {
        bool runnable = true;
        if (!exactBool(value, &runnable)) return false;
        if (m_dfnrRunnable != runnable) {
            m_dfnrRunnable = runnable;
            emit dfnrAvailabilityChanged();
        }
        return true;
    }
    if (name == "mnrRunnable") {
        bool runnable = true;
        if (!exactBool(value, &runnable)) return false;
        if (m_mnrRunnable != runnable) {
            m_mnrRunnable = runnable;
            emit mnrAvailabilityChanged();
        }
        return true;
    }
    if (name == "mnrStatus") {
        QString text;
        if (!exactString(value, &text)) return false;
        if (m_mnrStatus != text) {
            m_mnrStatus = text;
            emit mnrAvailabilityChanged();
        }
        return true;
    }
    if (name == "dfnrModelStatus") {
        QString text;
        if (!exactString(value, &text)) return false;
        if (m_dfnrStatus != text) {
            m_dfnrStatus = text;
            emit dfnrAvailabilityChanged();
        }
        return true;
    }
    if (name == "nr3ModelAsset" || name == "nr3ModelStatus") {
        QString text;
        if (!exactString(value, &text)) return false;
        QString& field = name == "nr3ModelAsset" ? m_nr3Selected : m_nr3Status;
        if (field != text) {
            field = text;
            emit nr3SelectionChanged();
        }
        return true;
    }
    bool changed = false;
    if (name == "nnrStandardAsset" || name == "nnrPremiumAsset") {
        QString text;
        if (!exactString(value, &text)) return false;
        const int slot = name == "nnrStandardAsset" ? 0 : 1;
        if (m_selected[slot] != text) { m_selected[slot] = text; changed = true; }
    } else if (name == "nnrModelSelectionPending") {
        bool pending = false;
        if (!exactBool(value, &pending)) return false;
        if (m_remotePending != pending) { m_remotePending = pending; changed = true; }
    } else if (name == "nnrModelStatus") {
        QString text;
        if (!exactString(value, &text)) return false;
        if (m_status != text) { m_status = text; changed = true; }
    } else if (name == "selectionRevision") {
        qint64 revision = 0;
        if (!exactInteger(value, &revision) || revision <= 0
            || revision > std::numeric_limits<quint32>::max()) return false;
        if (m_revision != static_cast<quint32>(revision)) {
            m_revision = static_cast<quint32>(revision);
            changed = true;
        }
    } else {
        return false;
    }
    if (changed) emit selectionChanged();
    return true;
}

} // namespace NereusSDR
