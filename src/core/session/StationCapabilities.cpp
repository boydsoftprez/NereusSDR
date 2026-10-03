// =================================================================
// src/core/session/StationCapabilities.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 18.
// See StationCapabilities.h for the design rationale.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30: RADE reason: radeReasonVersion, after
//               rxFilterLowPassVersion and before coreBuildInfo. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Shared-input filters (ruling (d)): rxFilterLowPassVersion,
//               after radioMicVersion and before coreBuildInfo. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Radio codec lane: radioMicVersion, after
//               rx2AttenuatorVersion and before coreBuildInfo. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Level Cal 2: rx2AttenuatorVersion, after the direct media
//               ladder and before coreBuildInfo. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: the direct media ladder: mediaDirectVersion and
//               mediaStunUrls, before coreBuildInfo. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-29: The Core's TCI server settings (JJ's ruling of 2026-09-28,
//               stationTciSettingsVersion 1). J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29: iPhone app plan Task 23 (R-IOS-09, audioQualityVersion 1):
//               a device's own Opus bitrate. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-29 - RADE status: radeStatusVersion, after radioModelsVersion,
//                only for a peer that declared radeStatus. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: capability
//                                    descriptor codec. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46: hpsdrModel, radioProtocol and
//                                    radioAddress entries. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46: radioHardwareVersion, last
//                                    in the same minor-11 block.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22:
//                                    remotePgxlControlVersion and
//                                    remoteRfKitControlVersion after it.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-48: stationTciVersion, last in the
//                minor-11 block. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: accessoryDataVersion, last in the
//                minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: remoteTgxlControlVersion, last in the
//                minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 12 (R-IOS-08): stationIdentityVersion,
//                last in the minor-11 block. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 13 (R-IOS-08): deviceAdminVersion, last
//                in the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 14 (R-IOS-08): pairingVersion, last in
//                the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 19 (R-IOS-06): stationCatalogVersion,
//                last in the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 20 (R-IOS-27): displayExtrasVersion,
//                last in the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 1): transmitSettingsVersion, last
//                in the minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): sessionHolderVersion,
//               last in the minor-11 block, for a peer that declared
//               sessionHolder. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02): remoteTxVersion, last,
//               sent only to a peer whose hello declared remoteTx; txPermitted
//               now the station transmit gate's answer. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan, desktop remote transmit (R-IOS-13,
//               R-R3-42): txRefusalCode, txRefusalReason and txRefusalFix
//               after remoteTxVersion. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13): txStateVersion,
//               after remoteTxVersion and only with it (the `txState`
//               object). J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
//   2026-09-25 - R-IOS-27, R-IOS-06: bandSelectVersion, last in the
//                minor-11 block. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-13 / R-R3-49 (parity Task 15): meterReadingsVersion,
//                after bandSelectVersion in the minor-11 block. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16):
//                dspInfoVersion, after meterReadingsVersion in the minor-11
//                block. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-26 - R-IOS-25 / R-R3-49 (parity Task 19): recordStreamVersion,
//                after dspInfoVersion in the minor-11 block. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-IOS-18 / R-R3-49 (parity Task 21): stationRadiosVersion,
//                after recordStreamVersion. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-26 - R-R3-49 / A11 (parity Task 28): txDisplayVersion, after
//                stationRadiosVersion. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-21 / R-R3-08: displayClockVersion, after
//                txDisplayVersion (merge with parity Task 28). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-IOS-16 (iPhone app plan Task 28 fix wave):
//                controlChannelVersion, after displayClockVersion (merged
//                into the trunk after txDisplayVersion and
//                displayClockVersion). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-27 - R-IOS-13 / R-R3-49 (parity Task 32): txMonitorAudioVersion,
//                after controlChannelVersion. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-27 - R-IOS-26 / R-R3-49 (iPhone plan Task 22, parity Task 20):
//                stationFreedvVersion, after txMonitorAudioVersion. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-16 (iPhone app plan Task 29): mediaReplaceVersion,
//                controlSwitchVersion and relayAllowed, after
//                stationFreedvVersion. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-27 - R-R3-49 / R-IOS-18 (remote-window parity Task 22, iPhone
//                app plan Task 25): supportBundleVersion, after
//                relayAllowed. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-27 - R-R3-49 / R-R3-32 (parity Task 33): txReadingsVersion,
//                right after txStateVersion and only with it. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-13 / R-R3-49: txModMonitorVersion, after
//                relayAllowed. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - R-IOS-13 / R-R3-49: txEqCurveVersion, after
//                radioAntennaRowsVersion, only for a peer that declared
//                txEqCurve. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Phone wire batch: diversityPatternVersion, after
//                vaxVersion, only for a peer that declared
//                diversityPattern. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - Phone wire batch: logCategoryListVersion, after
//                diversityPatternVersion, only for a peer that declared
//                logCategoryList. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - Phone wire batch: radioModelsVersion, after
//                logCategoryListVersion, only for a peer that declared
//                radioModels. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-29 - The phone's direct addresses: coreAddressesVersion, after
//                radioModelsVersion, only for a device signed in with its
//                own key that declared coreAddresses. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-11: adcAttenuatorVersion, after
//                txEqCurveVersion, only for a peer that declared
//                adcAttenuators. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: paProfileVersion and the read-only
//                 paProfiles object (PaProfilesFacade). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: txInhibitReasonVersion, after
//                paProfileVersion. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - Slice control plan Task 4: sliceAccessVersion, after
//                radioAntennaRowsVersion, only with sliceAccessEntry. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/StationCapabilities.h"

#include "core/BoardCapabilities.h"
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <limits>

namespace NereusSDR {

namespace {
bool validBuildText(const QString& value, qsizetype maxBytes, bool required)
{
    if ((required && value.isEmpty()) || value.size() > maxBytes) return false;
    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character.category() == QChar::Other_Control) return false;
        if (character.isHighSurrogate()) {
            if (index + 1 >= value.size() || !value.at(index + 1).isLowSurrogate()) return false;
            ++index;
        } else if (character.isLowSurrogate()) {
            return false;
        }
    }
    return value.toUtf8().size() <= maxBytes;
}
} // namespace

QByteArray CoreBuildInfo::toJson() const
{
    if (!validBuildText(productVersion, 128, true) || !validBuildText(sourceTag, 1024, false)) {
        return {};
    }
    const QByteArray json = QJsonDocument(QJsonObject{
        {QStringLiteral("productVersion"), productVersion},
        {QStringLiteral("sourceTag"), sourceTag}}).toJson(QJsonDocument::Compact);
    return json.size() <= 4096 ? json : QByteArray();
}

std::optional<CoreBuildInfo> CoreBuildInfo::fromJson(const QByteArray& json)
{
    if (json.isEmpty() || json.size() > 4096) return std::nullopt;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return std::nullopt;
    const QJsonObject object = document.object();
    const QJsonValue version = object.value(QStringLiteral("productVersion"));
    const QJsonValue tag = object.value(QStringLiteral("sourceTag"));
    if (!version.isString() || !tag.isString()) return std::nullopt;
    CoreBuildInfo identity{version.toString(), tag.toString()};
    return identity.toJson().isEmpty() ? std::nullopt : std::optional(identity);
}

namespace {

MirrorUpdate stringEntry(const char* name, const QString& value)
{
    return MirrorUpdate{ 0, QByteArray(name), MirrorWireKind::Utf8, QVariant(value) };
}

MirrorUpdate intEntry(const char* name, qint64 value)
{
    return MirrorUpdate{ 0, QByteArray(name), MirrorWireKind::Int64,
                         QVariant(static_cast<qlonglong>(value)) };
}

MirrorUpdate boolEntry(const char* name, bool value)
{
    return MirrorUpdate{ 0, QByteArray(name), MirrorWireKind::Bool, QVariant(value) };
}

// The direct media ladder: a STUN URL mediaStunUrls may carry. Never TURN,
// and nothing that could hold a credential or a token.
bool isMediaStunUrl(const QString& url)
{
    return (url.startsWith(QLatin1String("stun:")) || url.startsWith(QLatin1String("stuns:")))
        && url.size() <= StationCapabilities::kMaxMediaStunUrlBytes
        && !url.contains(QLatin1Char('@')) && !url.contains(QLatin1Char('?'));
}

} // namespace

QList<MirrorUpdate> StationCapabilities::toUpdates() const
{
    const bool hasBudget = remoteDisplayBudgetVersion > 0 && displayBudget
        && displayBudget->isValid();
    QList<MirrorUpdate> updates{
        stringEntry("stationName", stationName),
        stringEntry("radioModel", radioModelName),
        stringEntry("firmwareVersion", firmwareVersion),
        stringEntry("macAddress", macAddress),
        intEntry("board", static_cast<qint64>(board)),
        boolEntry("radioConnected", radioConnected),
        intEntry("effectiveMaxSlices", effectiveMaxSlices),
        intEntry("boardMaxSlices", boardMaxSlices),
        intEntry("userDdcCount", userDdcCount),
        boolEntry("pureSignalPresent", pureSignalPresent),
        boolEntry("txPermitted", txPermitted),
        intEntry("remoteMediaVersion", remoteMediaVersion),
        intEntry("remoteWidebandDisplayVersion", remoteWidebandDisplayVersion),
        intEntry("remoteAudioStatusVersion", remoteAudioStatusVersion),
        intEntry("spectrumGrantVersion", spectrumGrantVersion),
        intEntry("remoteDisplayBudgetVersion", hasBudget ? remoteDisplayBudgetVersion : 0),
        intEntry("remoteCtunVersion", remoteCtunVersion),
        intEntry("stationTelemetryVersion", stationTelemetryVersion),
        intEntry("remoteTgxlConfigVersion", remoteTgxlConfigVersion),
        intEntry("remoteFourO3AControlVersion", remoteFourO3AControlVersion),
        intEntry("wdspVersion", wdspVersion),
        intEntry("wdspCompatibilityVersion", wdspCompatibilityVersion),
        intEntry("nnrVersion", nnrVersion),
        intEntry("psAlgorithmVersion", psAlgorithmVersion),
        intEntry("propertyResultVersion", propertyResultVersion),
        intEntry("dspAssetVersion", dspAssetVersion),
        intEntry("psDisplayVersion", psDisplayVersion),
        intEntry("notchControlVersion", notchControlVersion),
        intEntry("audioProfileVersion", audioProfileVersion),
        intEntry("audioClockVersion", audioClockVersion),
        intEntry("receiverAudioVersion", receiverAudioVersion),
        intEntry("headphonesMixVersion", headphonesMixVersion),
        intEntry("settingsSchemaVersion", settingsSchemaVersion),
    };
    if (hasBudget) {
        updates.append(intEntry("displayApplicationBytesPerSecond",
                                static_cast<qint64>(displayBudget->applicationBytesPerSecond)));
        updates.append(intEntry("spectrumSampleUnitsPerSecond",
                                static_cast<qint64>(displayBudget->spectrumSampleUnitsPerSecond)));
        updates.append(intEntry("displayBudgetGeneration", displayBudget->generation));
        updates.append(boolEntry("remotePs3DisplaySubscribed", remotePs3DisplaySubscribed));
        if (displayBudgetReason) {
            updates.append(stringEntry("displayBudgetReason",
                                       displayBudgetReasonWireName(*displayBudgetReason)));
        }
    }
    // R-R3-46: last, so an app that negotiated them sees today's descriptor
    // followed by the three, and one that did not sees today's descriptor.
    if (radioIdentityEntries) {
        updates.append(intEntry("hpsdrModel", static_cast<qint64>(hpsdrModel)));
        updates.append(intEntry("radioProtocol", radioProtocol));
        updates.append(stringEntry("radioAddress", radioAddress));
        updates.append(intEntry("radioHardwareVersion", radioHardwareVersion));
        // R-R3-47 / R-R3-22: the Core's amplifier and RF-Kit status objects.
        updates.append(intEntry("remotePgxlControlVersion", remotePgxlControlVersion));
        updates.append(intEntry("remoteRfKitControlVersion", remoteRfKitControlVersion));
        // R-R3-48: the Core's station TCI server.
        updates.append(intEntry("stationTciVersion", stationTciVersion));
        // R-R3-47 / R-R3-22: the Core's accessory records and settings.
        updates.append(intEntry("accessoryDataVersion", accessoryDataVersion));
        // R-R3-47 / R-R3-22: the Tuner Genius's own settings.
        updates.append(intEntry("remoteTgxlControlVersion", remoteTgxlControlVersion));
        // iPhone app Task 12: device sign-in by key, last.
        updates.append(intEntry("stationIdentityVersion", stationIdentityVersion));
        // iPhone app Task 13: the devices object and its verbs.
        updates.append(intEntry("deviceAdminVersion", deviceAdminVersion));
        // iPhone app Task 14: pairing, the pairing window and its verbs.
        updates.append(intEntry("pairingVersion", pairingVersion));
        // iPhone app Task 19: the catalogue.
        updates.append(intEntry("stationCatalogVersion", stationCatalogVersion));
        // iPhone app Task 20: display extras.
        updates.append(intEntry("displayExtrasVersion", displayExtrasVersion));
        // R-R3-49 (parity Task 1): the transmit settings a receive-only
        // Core takes while the radio is off the air.
        updates.append(intEntry("transmitSettingsVersion", transmitSettingsVersion));
        // R-IOS-27, R-IOS-06: slice.selectBand.
        updates.append(intEntry("bandSelectVersion", bandSelectVersion));
        // R-R3-13 / R-R3-49 (parity Task 15): the ADC and AGC readings.
        updates.append(intEntry("meterReadingsVersion", meterReadingsVersion));
        // R-R3-49 / R-R3-21 / R-R3-40 (parity Task 16): the DSP facts.
        updates.append(intEntry("dspInfoVersion", dspInfoVersion));
        // R-IOS-25 / R-R3-49 (parity Task 19): the record streams.
        updates.append(intEntry("recordStreamVersion", recordStreamVersion));
        // R-IOS-18 / R-R3-49 (parity Task 21): the Core's radio choice.
        updates.append(intEntry("stationRadiosVersion", stationRadiosVersion));
        // R-R3-49 / A11 (parity Task 28): the transmit display.
        updates.append(intEntry("txDisplayVersion", txDisplayVersion));
        // R-R3-21 / R-R3-08: display frames and audio on one Core clock.
        updates.append(intEntry("displayClockVersion", displayClockVersion));
        // R-IOS-16 (Task 28 fix wave): the control session through the
        // remote access service.
        updates.append(intEntry("controlChannelVersion", controlChannelVersion));
        // R-IOS-13 / R-R3-49 (parity Task 32): the transmit monitor to the
        // device that holds transmit.
        updates.append(intEntry("txMonitorAudioVersion", txMonitorAudioVersion));
        // R-IOS-26 / R-R3-49 (iPhone plan Task 22, parity Task 20): the
        // Core's FreeDV Reporter.
        updates.append(intEntry("stationFreedvVersion", stationFreedvVersion));
        // iPhone app plan Task 29 (R-IOS-16): moving media and the session
        // to a better path, and whether the Core allows the relay.
        updates.append(intEntry("mediaReplaceVersion", mediaReplaceVersion));
        updates.append(intEntry("controlSwitchVersion", controlSwitchVersion));
        updates.append(boolEntry("relayAllowed", relayAllowed));
        // R-R3-49 / R-IOS-18 (parity Task 22, iPhone plan Task 25): the
        // support bundle, the Core's log and its logging categories.
        updates.append(intEntry("supportBundleVersion", supportBundleVersion));
        // iPhone app Task 71: several devices at once, only for
        // a peer that declared the feature.
        if (sessionHolderEntry) {
            updates.append(intEntry("sessionHolderVersion", sessionHolderVersion));
        }
        // iPhone app plan Task 34: remote transmit, only for a
        // peer that declared the feature.
        if (remoteTxEntry) {
            updates.append(intEntry("remoteTxVersion", remoteTxVersion));
            if (txWatchPathVersion > 0) {
                updates.append(intEntry("txWatchPathVersion", txWatchPathVersion));
            }
            // Desktop remote transmit: the Core's reason with it.
            updates.append(stringEntry("txRefusalCode", txRefusalCode));
            updates.append(stringEntry("txRefusalReason", txRefusalReason));
            updates.append(stringEntry("txRefusalFix", txRefusalFix));
            // iPhone app plan Task 39: the `txState` object, with it.
            updates.append(intEntry("txStateVersion", txStateVersion));
            // Remote-window parity Task 33 (R-R3-49): the transmit
            // readings (txState's raw forward and reflected power, the CFC
            // compression stream), right after txStateVersion and only with
            // it.
            updates.append(intEntry("txReadingsVersion", txReadingsVersion));
        }
        // Task 29 step 2b (R-IOS-16): unpublished media-floor entries
        // follow every previously emitted minor-11 field.
        updates.append(intEntry("mediaTunnelVersion", mediaTunnelVersion));
        updates.append(intEntry("mediaRelayRoutingVersion", mediaRelayRoutingVersion));
        if (settingsHygieneVersion > 0) {
            updates.append(intEntry("settingsHygieneVersion", settingsHygieneVersion));
        }
        if (settingsBackupVersion > 0) {
            updates.append(intEntry("settingsBackupVersion", settingsBackupVersion));
        }
        updates.append(intEntry("remoteIqVersion", remoteIqVersion));
        updates.append(intEntry("txModMonitorVersion", txModMonitorVersion));
        if (setupDescriptionVersion > 0) {
            updates.append(intEntry("setupDescriptionVersion", setupDescriptionVersion));
        }
        if (miniDisplayVersion > 0) {
            updates.append(intEntry("miniDisplayVersion", miniDisplayVersion));
        }
        updates.append(intEntry("accessoryTxVersion", accessoryTxVersion));
        if (radioAntennaRowsVersion == 1) {
            updates.append(intEntry("radioAntennaRowsVersion", radioAntennaRowsVersion));
        }
        // iPhone app plan Task 25: the station computer's VAX, only for a
        // peer that declared vax.
        if (vaxEntry) {
            updates.append(intEntry("vaxVersion", vaxVersion));
        }
        // R-IOS-13 / R-R3-49: the read-only TX EQ curve on `transmit`,
        // only for a peer that declared txEqCurve.
        if (txEqCurveVersion > 0) {
            updates.append(intEntry("txEqCurveVersion", txEqCurveVersion));
        }
        // R-IOS-26 / R-R3-49: 2 m, last, for a peer that declared band2m.
        if (band2mVersion == 1) {
            updates.append(intEntry("band2mVersion", band2mVersion));
        }
        // Phone wire batch: the Diversity dialog's pattern on each slice,
        // only for a peer that declared diversityPattern.
        if (diversityPatternVersion > 0) {
            updates.append(intEntry("diversityPatternVersion", diversityPatternVersion));
        }
        // Phone wire batch: radio's logCategoryList, only for a peer that
        // declared logCategoryList.
        if (logCategoryListVersion > 0) {
            updates.append(intEntry("logCategoryListVersion", logCategoryListVersion));
        }
        // Phone wire batch: stationRadios' model labels and choices, only
        // for a peer that declared radioModels.
        if (radioModelsEntry) {
            updates.append(intEntry("radioModelsVersion", radioModelsVersion));
        }
        // The phone's direct addresses: devices' coreAddresses, only for a
        // device signed in with its own key that declared coreAddresses.
        if (coreAddressesVersion > 0) {
            updates.append(intEntry("coreAddressesVersion", coreAddressesVersion));
        }
        // iPhone app plan Task 23 (R-IOS-09): a device's own Opus bitrate,
        // last, for a peer that declared audioQuality.
        if (audioQualityVersion == 1) {
            updates.append(intEntry("audioQualityVersion", audioQualityVersion));
        }
        // JJ's ruling of 2026-09-28: the Core's TCI server settings, last,
        // for a peer that declared stationTciSettings.
        if (stationTciSettingsVersion == 1) {
            updates.append(intEntry("stationTciSettingsVersion", stationTciSettingsVersion));
        }
        // R-R3-46 / R-R3-11: the other ADC's own attenuator on `stepAtt`,
        // only for a peer that declared adcAttenuators.
        if (adcAttenuatorVersion > 0) {
            updates.append(intEntry("adcAttenuatorVersion", adcAttenuatorVersion));
        }
        // R-R3-49 / R-IOS-18: the PA Gain profiles, only for a peer that
        // declared paProfiles.
        if (paProfileVersion > 0) {
            updates.append(intEntry("paProfileVersion", paProfileVersion));
        }
        // RADE status: each slice's radeSynced and radeFreqOffsetHz, only
        // for a peer that declared radeStatus.
        if (radeStatusVersion > 0) {
            updates.append(intEntry("radeStatusVersion", radeStatusVersion));
        }
        // HL2 port part 2: radio's txInhibitReason, only for a peer that
        // declared txInhibitReason.
        if (txInhibitReasonVersion > 0) {
            updates.append(intEntry("txInhibitReasonVersion", txInhibitReasonVersion));
        }
        // PA on-air gate re-review: radio's paTransmitBand, only for a peer
        // that declared paTransmitBand.
        if (paTransmitBandVersion > 0) {
            updates.append(intEntry("paTransmitBandVersion", paTransmitBandVersion));
        }
        // Slice control plan Task 4: shared listening and control handoff,
        // appended after paTransmitBandVersion, only for a peer that
        // declared sliceAccess.
        if (sliceAccessEntry) {
            updates.append(intEntry("sliceAccessVersion", sliceAccessVersion));
        }
    }
    // The direct media ladder: after sliceAccessVersion and before
    // coreBuildInfo (which stays last), only for a peer that declared
    // mediaDirect; an older peer's descriptor is unchanged.
    if (mediaDirectVersion > 0) {
        updates.append(intEntry("mediaDirectVersion", mediaDirectVersion));
        QJsonArray urls;
        for (const QString& url : mediaStunUrls) {
            if (isMediaStunUrl(url) && urls.size() < kMaxMediaStunUrls) {
                urls.append(url);
            }
        }
        updates.append(stringEntry(
            "mediaStunUrls",
            QString::fromUtf8(QJsonDocument(urls).toJson(QJsonDocument::Compact))));
    }
    // Level Cal 2: RX2's input control in the catalogue, after the direct
    // media ladder and before coreBuildInfo (which stays last), only for a
    // peer that declared rx2Attenuator; an older peer's descriptor is
    // unchanged.
    if (rx2AttenuatorVersion > 0) {
        updates.append(intEntry("rx2AttenuatorVersion", rx2AttenuatorVersion));
    }
    // Radio codec lane: the catalogue's radio mic keys, after
    // rx2AttenuatorVersion and before coreBuildInfo (which stays last),
    // only for a peer that declared radioMic.
    if (radioMicVersion > 0) {
        updates.append(intEntry("radioMicVersion", radioMicVersion));
    }
    // Shared-input filters, ruling (d): radio's low-pass reason fields,
    // after radioMicVersion and before coreBuildInfo (which stays last),
    // only for a peer that declared rxFilterLowPass.
    if (rxFilterLowPassVersion > 0) {
        updates.append(intEntry("rxFilterLowPassVersion", rxFilterLowPassVersion));
    }
    // RADE reason: each slice's radeReason, after rxFilterLowPassVersion and
    // before coreBuildInfo (which stays last), only for a peer that declared
    // radeReason.
    if (radeReasonVersion > 0) {
        updates.append(intEntry("radeReasonVersion", radeReasonVersion));
    }
    if (coreBuildInfo) {
        const QByteArray json = coreBuildInfo->toJson();
        if (!json.isEmpty()) updates.append(stringEntry("coreBuildInfo", QString::fromUtf8(json)));
    }
    return updates;
}

StationCapabilities StationCapabilities::fromUpdates(const QList<MirrorUpdate>& updates)
{
    StationCapabilities caps;
    DisplayBudgetLimits budget;
    QSet<QByteArray> budgetFields;
    bool invalidBudget = false;
    int reasonEntries = 0;
    std::optional<DisplayBudgetReason> reason;
    int buildInfoEntries = 0;
    std::optional<CoreBuildInfo> buildInfo;
    int mediaDirectEntries = 0;
    int mediaStunEntries = 0;
    int rx2AttenuatorEntries = 0;
    int radioMicEntries = 0;
    int rxFilterLowPassEntries = 0;
    int radeReasonEntries = 0;
    for (const MirrorUpdate& u : updates) {
        if (u.name == "mediaDirectVersion") {
            // The direct media ladder: one entry, an Int64 of 1 or more.
            if (++mediaDirectEntries == 1 && u.ordinal == 0 && u.kind == MirrorWireKind::Int64
                && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong version = u.value.toLongLong();
                caps.mediaDirectVersion = version > 0 && version <= 65535
                    ? static_cast<int>(version) : 0;
            }
        } else if (u.name == "rxFilterLowPassVersion") {
            // Shared-input filters, ruling (d): one entry, an Int64 of 1 or
            // more.
            if (++rxFilterLowPassEntries == 1 && u.ordinal == 0
                && u.kind == MirrorWireKind::Int64
                && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong version = u.value.toLongLong();
                caps.rxFilterLowPassVersion = version > 0 && version <= 65535
                    ? static_cast<int>(version) : 0;
            }
        } else if (u.name == "radeReasonVersion") {
            // RADE reason: one entry, an Int64 of 1 or more.
            if (++radeReasonEntries == 1 && u.ordinal == 0
                && u.kind == MirrorWireKind::Int64
                && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong version = u.value.toLongLong();
                caps.radeReasonVersion = version > 0 && version <= 65535
                    ? static_cast<int>(version) : 0;
            }
        } else if (u.name == "radioMicVersion") {
            // Radio codec lane: one entry, an Int64 of 1 or more.
            if (++radioMicEntries == 1 && u.ordinal == 0
                && u.kind == MirrorWireKind::Int64
                && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong version = u.value.toLongLong();
                caps.radioMicVersion = version > 0 && version <= 65535
                    ? static_cast<int>(version) : 0;
            }
        } else if (u.name == "rx2AttenuatorVersion") {
            // Level Cal 2: one entry, an Int64 of 1 or more.
            if (++rx2AttenuatorEntries == 1 && u.ordinal == 0
                && u.kind == MirrorWireKind::Int64
                && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong version = u.value.toLongLong();
                caps.rx2AttenuatorVersion = version > 0 && version <= 65535
                    ? static_cast<int>(version) : 0;
            }
        } else if (u.name == "mediaStunUrls") {
            if (++mediaStunEntries == 1 && u.ordinal == 0 && u.kind == MirrorWireKind::Utf8
                && u.value.typeId() == QMetaType::QString && u.value.toString().size() <= 4096) {
                const QJsonDocument document =
                    QJsonDocument::fromJson(u.value.toString().toUtf8());
                const QJsonArray urls = document.isArray() ? document.array() : QJsonArray{};
                for (const QJsonValue& url : urls) {
                    if (caps.mediaStunUrls.size() >= kMaxMediaStunUrls) {
                        break;
                    }
                    if (url.isString() && isMediaStunUrl(url.toString())) {
                        caps.mediaStunUrls.append(url.toString());
                    }
                }
            }
        } else if (u.name == "coreBuildInfo") {
            ++buildInfoEntries;
            if (buildInfoEntries == 1 && u.ordinal == 0 && u.kind == MirrorWireKind::Utf8
                && u.value.typeId() == QMetaType::QString) {
                const QString text = u.value.toString();
                if (text.size() <= 4096) buildInfo = CoreBuildInfo::fromJson(text.toUtf8());
            }
        } else if (u.name == "displayBudgetReason") {
            // Not one of the five budget fields: an older app ignores it,
            // and a bad reason never costs this app its budget.
            ++reasonEntries;
            if (u.kind == MirrorWireKind::Utf8 && u.value.typeId() == QMetaType::QString) {
                reason = displayBudgetReasonFromWireName(u.value.toString());
            }
        } else if (u.name == "remoteDisplayBudgetVersion"
            || u.name == "displayApplicationBytesPerSecond"
            || u.name == "spectrumSampleUnitsPerSecond"
            || u.name == "displayBudgetGeneration"
            || u.name == "remotePs3DisplaySubscribed") {
            if (budgetFields.contains(u.name)) { invalidBudget = true; }
            budgetFields.insert(u.name);
            if (u.name == "remotePs3DisplaySubscribed") {
                if (u.kind != MirrorWireKind::Bool || u.value.typeId() != QMetaType::Bool) {
                    invalidBudget = true;
                } else {
                    caps.remotePs3DisplaySubscribed = u.value.toBool();
                }
                continue;
            }
            if (u.kind != MirrorWireKind::Int64 || u.value.typeId() != QMetaType::LongLong) {
                invalidBudget = true;
                continue;
            }
            const qint64 value = u.value.toLongLong();
            if (u.name == "remoteDisplayBudgetVersion") {
                if (value < 0 || value > 65535) { invalidBudget = true; }
                else { caps.remoteDisplayBudgetVersion = static_cast<int>(value); }
            } else if (u.name == "displayBudgetGeneration") {
                if (value <= 0 || quint64(value) > std::numeric_limits<quint32>::max()) {
                    invalidBudget = true;
                } else { budget.generation = static_cast<quint32>(value); }
            } else if (value <= 0) {
                invalidBudget = true;
            } else if (u.name == "displayApplicationBytesPerSecond") {
                budget.applicationBytesPerSecond = static_cast<quint64>(value);
            } else {
                budget.spectrumSampleUnitsPerSecond = static_cast<quint64>(value);
            }
        } else if (u.name == "stationName") {
            caps.stationName = u.value.toString();
        } else if (u.name == "radioModel") {
            caps.radioModelName = u.value.toString();
        } else if (u.name == "firmwareVersion") {
            caps.firmwareVersion = u.value.toString();
        } else if (u.name == "macAddress") {
            caps.macAddress = u.value.toString();
        } else if (u.name == "board") {
            // A board integer this build has never heard of resolves to
            // Unknown rather than being kept as an enum value no switch
            // here handles. HPSDRHW's values are a sparse, deliberately-
            // reserved range (HpsdrModel.h reserves 7..9 and 13..19), so a
            // newer daemon's SKU number can land in a hole this build has
            // no row for. The cast itself is well defined (HPSDRHW has a
            // fixed underlying type), so the check is semantic, not a
            // UB guard: BoardCapsTable::forBoard() falls back to the
            // Unknown row for anything it does not carry, and comparing
            // the row it hands back against what was asked for is the one
            // available way to tell "recognised" from "fell back" without
            // hand-copying the enum list into a second place that can
            // drift. Every consumer already has a fallback for Unknown.
            const auto candidate = static_cast<HPSDRHW>(u.value.toLongLong());
            caps.board = BoardCapsTable::forBoard(candidate).board == candidate
                             ? candidate
                             : HPSDRHW::Unknown;
        } else if (u.name == "radioConnected") {
            caps.radioConnected = u.value.toBool();
        } else if (u.name == "effectiveMaxSlices") {
            caps.effectiveMaxSlices = static_cast<int>(u.value.toLongLong());
        } else if (u.name == "boardMaxSlices") {
            caps.boardMaxSlices = static_cast<int>(u.value.toLongLong());
        } else if (u.name == "userDdcCount") {
            caps.userDdcCount = static_cast<int>(u.value.toLongLong());
        } else if (u.name == "pureSignalPresent") {
            caps.pureSignalPresent = u.value.toBool();
        } else if (u.name == "txPermitted") {
            caps.txPermitted = u.value.toBool();
        } else if (u.name == "remoteMediaVersion") {
            const qlonglong version = u.value.toLongLong();
            caps.remoteMediaVersion = version >= 0 && version <= 65535
                ? static_cast<int>(version) : 0;
        } else if (u.name == "remoteWidebandDisplayVersion") {
            const qlonglong version = u.value.toLongLong();
            caps.remoteWidebandDisplayVersion = version >= 0 && version <= 65535
                ? static_cast<int>(version) : 0;
        } else if (u.name == "remoteAudioStatusVersion") {
            const qlonglong version = u.value.toLongLong();
            caps.remoteAudioStatusVersion = version >= 0 && version <= 65535
                ? static_cast<int>(version) : 0;
        } else if (u.name == "spectrumGrantVersion") {
            const qlonglong version = u.value.toLongLong();
            caps.spectrumGrantVersion = version >= 0 && version <= 65535
                ? static_cast<int>(version) : 0;
        } else if (u.name == "remoteCtunVersion") {
            const qlonglong version = u.value.toLongLong();
            caps.remoteCtunVersion = version >= 0 && version <= 65535
                ? static_cast<int>(version) : 0;
        } else if (u.name == "stationTelemetryVersion") {
            const qlonglong version = u.value.toLongLong();
            caps.stationTelemetryVersion = version >= 0 && version <= 65535
                ? static_cast<int>(version) : 0;
        } else if (u.name == "remoteTgxlConfigVersion") {
            const qlonglong version = u.value.toLongLong();
            caps.remoteTgxlConfigVersion = version >= 0 && version <= 65535
                ? static_cast<int>(version) : 0;
        } else if (u.name == "remoteFourO3AControlVersion") {
            const qlonglong version = u.value.toLongLong();
            caps.remoteFourO3AControlVersion = version >= 0 && version <= 65535
                ? static_cast<int>(version) : 0;
        } else if (u.name == "wdspVersion" || u.name == "wdspCompatibilityVersion"
                   || u.name == "nnrVersion" || u.name == "psAlgorithmVersion"
                   || u.name == "propertyResultVersion" || u.name == "dspAssetVersion"
                   || u.name == "psDisplayVersion" || u.name == "notchControlVersion"
                   || u.name == "audioProfileVersion" || u.name == "audioClockVersion"
                   || u.name == "receiverAudioVersion"
                   || u.name == "headphonesMixVersion") {
            const qlonglong raw = u.value.toLongLong();
            const int version = raw >= 0 && raw <= 65535 ? static_cast<int>(raw) : 0;
            if (u.name == "wdspVersion") caps.wdspVersion = version;
            else if (u.name == "wdspCompatibilityVersion") caps.wdspCompatibilityVersion = version;
            else if (u.name == "nnrVersion") caps.nnrVersion = version;
            else if (u.name == "psAlgorithmVersion") caps.psAlgorithmVersion = version;
            else if (u.name == "propertyResultVersion") caps.propertyResultVersion = version;
            else if (u.name == "dspAssetVersion") caps.dspAssetVersion = version;
            else if (u.name == "notchControlVersion") caps.notchControlVersion = version;
            else if (u.name == "audioProfileVersion") caps.audioProfileVersion = version;
            else if (u.name == "audioClockVersion") caps.audioClockVersion = version;
            else if (u.name == "receiverAudioVersion") caps.receiverAudioVersion = version;
            else if (u.name == "headphonesMixVersion") caps.headphonesMixVersion = version;
            else caps.psDisplayVersion = version;
        } else if (u.name == "hpsdrModel") {
            // R-R3-46. A model this build has never heard of (a newer Core's
            // SKU) reads as not reported, so the window falls back to the
            // board alone rather than holding an enum value no switch here
            // handles.
            caps.radioIdentityEntries = true;
            if (u.kind == MirrorWireKind::Int64 && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong raw = u.value.toLongLong();
                caps.hpsdrModel = raw > static_cast<qlonglong>(HPSDRModel::FIRST)
                        && raw < static_cast<qlonglong>(HPSDRModel::LAST)
                    ? static_cast<HPSDRModel>(raw) : HPSDRModel::FIRST;
            }
        } else if (u.name == "radioProtocol") {
            caps.radioIdentityEntries = true;
            if (u.kind == MirrorWireKind::Int64 && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong raw = u.value.toLongLong();
                caps.radioProtocol = raw == 1 || raw == 2 ? static_cast<int>(raw) : 0;
            }
        } else if (u.name == "radioAddress") {
            caps.radioIdentityEntries = true;
            if (u.kind == MirrorWireKind::Utf8 && u.value.typeId() == QMetaType::QString) {
                // Only an address: anything else is not shown as one.
                const QString text = u.value.toString().trimmed();
                caps.radioAddress = QHostAddress(text).isNull() ? QString() : text;
            }
        } else if (u.name == "radioHardwareVersion") {
            // R-R3-46: sent in the same block as the three above.
            caps.radioIdentityEntries = true;
            if (u.kind == MirrorWireKind::Int64 && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong raw = u.value.toLongLong();
                caps.radioHardwareVersion = raw >= 0 && raw <= 65535 ? static_cast<int>(raw) : 0;
            }
        } else if (u.name == "remotePgxlControlVersion"
                   || u.name == "remoteRfKitControlVersion"
                   || u.name == "stationTciVersion"
                   || u.name == "accessoryDataVersion"
                   || u.name == "accessoryTxVersion"
                   || u.name == "remoteTgxlControlVersion"
                   || u.name == "stationIdentityVersion"
                   || u.name == "deviceAdminVersion"
                   || u.name == "pairingVersion"
                   || u.name == "stationCatalogVersion"
                   || u.name == "setupDescriptionVersion"
                   || u.name == "displayExtrasVersion"
                   || u.name == "transmitSettingsVersion"
                   || u.name == "bandSelectVersion"
                   || u.name == "meterReadingsVersion"
                   || u.name == "dspInfoVersion"
                   || u.name == "recordStreamVersion"
                   || u.name == "stationRadiosVersion"
                   || u.name == "settingsHygieneVersion"
                   || u.name == "settingsBackupVersion"
                   || u.name == "txDisplayVersion"
                   || u.name == "displayClockVersion"
                   || u.name == "controlChannelVersion"
                   || u.name == "txMonitorAudioVersion"
                   || u.name == "stationFreedvVersion"
                   || u.name == "mediaReplaceVersion"
                   || u.name == "controlSwitchVersion"
                   || u.name == "supportBundleVersion"
                   || u.name == "mediaTunnelVersion"
                   || u.name == "mediaRelayRoutingVersion"
                   || u.name == "txModMonitorVersion"
                   || u.name == "sessionHolderVersion"
                   || u.name == "remoteTxVersion"
                   || u.name == "txWatchPathVersion"
                   || u.name == "txStateVersion"
                   || u.name == "txReadingsVersion"
                   || u.name == "remoteIqVersion"
                   || u.name == "miniDisplayVersion"
                   || u.name == "radioAntennaRowsVersion"
                   || u.name == "vaxVersion"
                   || u.name == "txEqCurveVersion"
                   || u.name == "band2mVersion"
                   || u.name == "diversityPatternVersion"
                   || u.name == "logCategoryListVersion"
                   || u.name == "radioModelsVersion"
                   || u.name == "coreAddressesVersion"
                   || u.name == "audioQualityVersion"
                   || u.name == "stationTciSettingsVersion"
                   || u.name == "adcAttenuatorVersion"
                   || u.name == "paProfileVersion"
                   || u.name == "radeStatusVersion"
                   || u.name == "txInhibitReasonVersion"
                   || u.name == "paTransmitBandVersion"
                   || u.name == "sliceAccessVersion") {
            // R-R3-47 / R-R3-22 / R-R3-48: sent in the same block as the
            // four above.
            caps.radioIdentityEntries = true;
            if (u.kind == MirrorWireKind::Int64 && u.value.typeId() == QMetaType::LongLong) {
                const qlonglong raw = u.value.toLongLong();
                const int version = raw >= 0 && raw <= 65535 ? static_cast<int>(raw) : 0;
                if (u.name == "remotePgxlControlVersion") {
                    caps.remotePgxlControlVersion = version;
                } else if (u.name == "remoteRfKitControlVersion") {
                    caps.remoteRfKitControlVersion = version;
                } else if (u.name == "stationTciVersion") {
                    caps.stationTciVersion = version;
                } else if (u.name == "accessoryDataVersion") {
                    caps.accessoryDataVersion = version;
                } else if (u.name == "accessoryTxVersion") {
                    caps.accessoryTxVersion = version;
                } else if (u.name == "radioAntennaRowsVersion") {
                    caps.radioAntennaRowsVersion = version == 1 ? 1 : 0;
                } else if (u.name == "vaxVersion") {
                    caps.vaxEntry = true;
                    caps.vaxVersion = version;
                } else if (u.name == "txEqCurveVersion") {
                    caps.txEqCurveVersion = version;
                } else if (u.name == "band2mVersion") {
                    caps.band2mVersion = version >= 1 ? 1 : 0;
                } else if (u.name == "diversityPatternVersion") {
                    caps.diversityPatternVersion = version;
                } else if (u.name == "logCategoryListVersion") {
                    caps.logCategoryListVersion = version;
                } else if (u.name == "radeStatusVersion") {
                    caps.radeStatusVersion = version;
                } else if (u.name == "paTransmitBandVersion") {
                    caps.paTransmitBandVersion = version;
                } else if (u.name == "radioModelsVersion") {
                    caps.radioModelsEntry = true;
                    caps.radioModelsVersion = version;
                } else if (u.name == "coreAddressesVersion") {
                    caps.coreAddressesVersion = version;
                } else if (u.name == "audioQualityVersion") {
                    caps.audioQualityVersion = version >= 1 ? 1 : 0;
                } else if (u.name == "stationTciSettingsVersion") {
                    caps.stationTciSettingsVersion = version >= 1 ? 1 : 0;
                } else if (u.name == "adcAttenuatorVersion") {
                    caps.adcAttenuatorVersion = version;
                } else if (u.name == "paProfileVersion") {
                    caps.paProfileVersion = version;
                } else if (u.name == "txInhibitReasonVersion") {
                    caps.txInhibitReasonVersion = version;
                } else if (u.name == "sliceAccessVersion") {
                    caps.sliceAccessEntry = true;
                    caps.sliceAccessVersion = version;
                } else if (u.name == "stationIdentityVersion") {
                    caps.stationIdentityVersion = version;
                } else if (u.name == "deviceAdminVersion") {
                    caps.deviceAdminVersion = version;
                } else if (u.name == "pairingVersion") {
                    caps.pairingVersion = version;
                } else if (u.name == "stationCatalogVersion") {
                    caps.stationCatalogVersion = version;
                } else if (u.name == "setupDescriptionVersion") {
                    caps.setupDescriptionVersion = version;
                } else if (u.name == "displayExtrasVersion") {
                    caps.displayExtrasVersion = version;
                } else if (u.name == "sessionHolderVersion") {
                    caps.sessionHolderEntry = true;
                    caps.sessionHolderVersion = version;
                } else if (u.name == "remoteTxVersion") {
                    caps.remoteTxEntry = true;
                    caps.remoteTxVersion = version;
                } else if (u.name == "txWatchPathVersion") {
                    caps.txWatchPathVersion = version;
                } else if (u.name == "txStateVersion") {
                    caps.txStateVersion = version;
                } else if (u.name == "txReadingsVersion") {
                    caps.txReadingsVersion = version;
                } else if (u.name == "remoteTgxlControlVersion") {
                    caps.remoteTgxlControlVersion = version;
                } else if (u.name == "transmitSettingsVersion") {
                    caps.transmitSettingsVersion = version;
                } else if (u.name == "meterReadingsVersion") {
                    caps.meterReadingsVersion = version;
                } else if (u.name == "dspInfoVersion") {
                    caps.dspInfoVersion = version;
                } else if (u.name == "recordStreamVersion") {
                    caps.recordStreamVersion = version;
                } else if (u.name == "stationRadiosVersion") {
                    caps.stationRadiosVersion = version;
                } else if (u.name == "settingsHygieneVersion") {
                    caps.settingsHygieneVersion = version;
                } else if (u.name == "settingsBackupVersion") {
                    caps.settingsBackupVersion = version;
                } else if (u.name == "txDisplayVersion") {
                    caps.txDisplayVersion = version;
                } else if (u.name == "displayClockVersion") {
                    caps.displayClockVersion = version;
                } else if (u.name == "controlChannelVersion") {
                    caps.controlChannelVersion = version;
                } else if (u.name == "txMonitorAudioVersion") {
                    caps.txMonitorAudioVersion = version;
                } else if (u.name == "remoteIqVersion") {
                    caps.remoteIqVersion = version;
                } else if (u.name == "miniDisplayVersion") {
                    caps.miniDisplayVersion = version;
                } else if (u.name == "stationFreedvVersion") {
                    caps.stationFreedvVersion = version;
                } else if (u.name == "mediaReplaceVersion") {
                    caps.mediaReplaceVersion = version;
                } else if (u.name == "controlSwitchVersion") {
                    caps.controlSwitchVersion = version;
                } else if (u.name == "supportBundleVersion") {
                    caps.supportBundleVersion = version;
                } else if (u.name == "mediaTunnelVersion") {
                    caps.mediaTunnelVersion = version;
                } else if (u.name == "mediaRelayRoutingVersion") {
                    caps.mediaRelayRoutingVersion = version;
                } else if (u.name == "txModMonitorVersion") {
                    caps.txModMonitorVersion = version;
                } else {
                    caps.bandSelectVersion = version;
                }
            }
        } else if (u.name == "relayAllowed") {
            // iPhone app plan Task 29: anything but a bool reads as allowed,
            // the setting's default.
            caps.radioIdentityEntries = true;
            caps.relayAllowedEntry = true;
            caps.relayAllowed = !(u.kind == MirrorWireKind::Bool
                                  && u.value.typeId() == QMetaType::Bool
                                  && !u.value.toBool());
        } else if (u.name == "txRefusalCode" || u.name == "txRefusalReason"
                   || u.name == "txRefusalFix") {
            // Desktop remote transmit: text only; anything else reads empty.
            const QString text = u.kind == MirrorWireKind::Utf8
                    && u.value.typeId() == QMetaType::QString
                ? u.value.toString()
                : QString();
            if (u.name == "txRefusalCode") {
                caps.txRefusalCode = text;
            } else if (u.name == "txRefusalReason") {
                caps.txRefusalReason = text;
            } else {
                caps.txRefusalFix = text;
            }
        } else if (u.name == "settingsSchemaVersion") {
            caps.settingsSchemaVersion = static_cast<qint32>(u.value.toLongLong());
        }
        // Anything else: ignored on purpose. See fromUpdates()'s doc
        // comment -- a newer daemon advertising more is the expected
        // forward-compatible case, not an error.
    }
    if (!invalidBudget && budgetFields.size() == 5
        && caps.remoteDisplayBudgetVersion > 0 && budget.isValid()) {
        caps.displayBudget = budget;
        if (reasonEntries == 1) {
            caps.displayBudgetReason = reason;
        }
    } else {
        caps.remoteDisplayBudgetVersion = 0;
        caps.remotePs3DisplaySubscribed = false;
    }
    if (buildInfoEntries == 1) caps.coreBuildInfo = buildInfo;
    return caps;
}

} // namespace NereusSDR
