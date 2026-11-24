// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/core/KiwiSdrManager.h [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Das KiwiSDR-Protokoll stammt von John Seamons (ZL/KF6VO),
// http://kiwisdr.com.
//
// Modification history (Longpath):
//   2026-08-23 — Portiert (Nachtschicht, Stufe 3: Bedienflaeche).
//                Namensraum und Kopfdatei-Pfade angepasst, sonst
//                zeichengetreu.
//   2026-09-21 — Longpath: Signal sliceTrackingUpdated, damit die
//                Bruecke im MainWindow (und ihr Pruefstand) sehen, was
//                dem Kiwi zuletzt nachgefuehrt wurde.

#pragma once

#include "core/KiwiSdrClient.h"
#include "core/KiwiSdrCredentialStore.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

class QTimer;
class QThread;

namespace Longpath {

struct KiwiSdrAntennaProfile {
    QString id;
    QString name;
    QString endpoint;
    bool autoConnect{false};
    // Keep this receiver's audio audible while the radio transmits (the
    // stream itself always keeps flowing during TX; this only opens the
    // transmit gate at the audio mix). Default off: muted during TX.
    bool keepAudioDuringTx{false};
    // Hold the post-TX unmute for the receiver's stream latency so the
    // resume lands on audio received after unkey (suppresses hearing your
    // own delayed TX tail on an in-range receiver). No effect when
    // keepAudioDuringTx is set.
    bool resumeAudioAfterTxDelay{false};
    bool waterfallAutoScale{true};
    int waterfallMinDbm{-110};
    int waterfallMaxDbm{-10};
    int waterfallRate{0};
    // Show this receiver's waterfall in the standalone "KIWI-WASSERFÄLLE"
    // panel, independent of any slice assignment. For a profile with no
    // slice assignment, this is what keeps it connected at all — see
    // KiwiSdrManager::connectProfile and shouldMaintainProfileConnection.
    bool waterfallPreviewEnabled{false};
};

struct KiwiSdrWaterfallDisplayRange {
    float minDbm{0.0f};
    float maxDbm{0.0f};
    bool autoRange{false};
    bool valid{false};
};

struct KiwiSdrCsvImportResult {
    int addedCount{0};
    int mergedCount{0};
    QStringList errors;
    bool ok() const { return errors.isEmpty(); }
};

struct KiwiSdrCsvExportResult {
    int exportedCount{0};
    QString error;
    bool ok() const { return error.isEmpty(); }
};

enum class KiwiSdrPasswordPersistenceState {
    Loading,
    NoPassword,
    Saving,
    Stored,
    SessionOnly,
    Error,
};

class KiwiSdrManager : public QObject {
    Q_OBJECT

public:
    explicit KiwiSdrManager(
        QObject* parent = nullptr,
        std::shared_ptr<IKiwiSdrCredentialStore> credentialStore = {});
    ~KiwiSdrManager() override;

    QVector<KiwiSdrAntennaProfile> profiles() const { return m_profiles; }
    KiwiSdrAntennaProfile profile(const QString& id) const;
    bool hasProfile(const QString& id) const;
    // Longpath (2026-11-24): das Profil mit diesem Endpunkt, leer wenn keins.
    // Schreibweise wie bei addProfile normalisiert.
    QString profileIdForEndpoint(const QString& endpoint) const;

    // CSV import/export of the saved receiver list (#4586), scoped to the
    // profile fields only — passwords stay in IKiwiSdrCredentialStore and
    // are never written to a plaintext file. Import merges: a row whose
    // endpoint matches an existing profile updates that profile in place
    // (keeping its id and password) rather than creating a duplicate.
    QByteArray exportProfilesCsv() const;
    KiwiSdrCsvImportResult importProfilesCsv(const QByteArray& bytes);
    KiwiSdrCsvExportResult exportToFile(const QString& path) const;
    KiwiSdrCsvImportResult importFromFile(const QString& path);
    QString displayName(const QString& id) const;
    QString virtualAntennaToken(const QString& id) const;
    QString profileIdForVirtualAntennaToken(const QString& token) const;
    QStringList virtualAntennaTokens() const;
    QStringList virtualAntennaLabels() const;

    KiwiSdrClient::State state(const QString& id) const;
    QString stateDetail(const QString& id) const;
    KiwiSdrReceiverTelemetry telemetry(const QString& id) const;
    KiwiSdrProtocol::ReceiverMetadata receiverMetadata(const QString& id) const;
    KiwiSdrProtocol::ProtocolState protocolState(const QString& id) const;
    bool waterfallAvailable(const QString& id) const;
    QString waterfallDetail(const QString& id) const;
    KiwiSdrWaterfallDisplayRange waterfallDisplayRange(
        const QString& id) const;
    bool isConnected(const QString& id) const;
    bool reconnectRecommended(const QString& id) const;
    bool waterfallPreviewEnabled(const QString& id) const;

    QString assignedProfileForSlice(int sliceId) const;
    int assignedSliceForProfile(const QString& id) const;
    QString profilePassword(const QString& id) const;
    bool isProfilePasswordLoaded(const QString& id) const;
    KiwiSdrPasswordPersistenceState profilePasswordPersistenceState(
        const QString& id) const;
    QString profilePasswordPersistenceDetail(const QString& id) const;

public slots:
    QString addProfile(const QString& name, const QString& endpoint);
    void updateProfile(const KiwiSdrAntennaProfile& profile);
    void setProfilePassword(const QString& id, const QString& password);
    void removeProfile(const QString& id);
    void connectProfile(const QString& id);
    void disconnectProfile(const QString& id);
    void disconnectAll();
    void setOperatorCallsign(const QString& callsign);
    void startAutoConnect();
    void primeProfileTracking(const QString& id, int sliceId,
                              double frequencyMhz, const QString& mode,
                              int filterLowHz, int filterHighHz,
                              const QString& panId, double centerMhz,
                              double bandwidthMhz, int lineDurationMs,
                              const QString& bandName, int cwPitchHz);
    void assignSliceToProfile(int sliceId, const QString& profileId,
                              double frequencyMhz, const QString& mode,
                              int filterLowHz, int filterHighHz,
                              const QString& panId,
                              const QString& bandName, int cwPitchHz);
    void clearSliceAssignment(int sliceId);
    void updateSliceTracking(int sliceId, double frequencyMhz,
                             const QString& mode, int filterLowHz,
                             int filterHighHz, const QString& panId,
                             const QString& bandName, int cwPitchHz);
    void updateWaterfallView(int sliceId, const QString& panId,
                             double centerMhz, double bandwidthMhz,
                             int lineDurationMs);
    void setReceiverControlsForSlice(
        int sliceId, const KiwiSdrReceiverControls& controls);
    void setProfileWaterfallDisplayRange(const QString& id, int minDbm,
                                         int maxDbm, bool autoScale,
                                         int rate);
    void requestProfileWaterfallAutoScale(const QString& id);
    // Toggles the standalone waterfall preview (Radio -> KiwiSDR panel
    // "KIWI-WASSERFÄLLE"). Enabling it connects the profile if nothing else
    // has (see connectProfile()'s waterfallPreviewEnabled branch); disabling
    // it disconnects the profile again, but only if nothing else still needs
    // the connection (shouldMaintainProfileConnection) — a profile that is
    // also assigned to a slice, or has autoConnect set, stays connected.
    void setWaterfallPreviewEnabled(const QString& id, bool enabled);

signals:
    void profilesChanged();
    void profilePasswordChanged(const QString& id);
    void profilePasswordPersistenceChanged(
        const QString& id,
        Longpath::KiwiSdrPasswordPersistenceState state,
        const QString& detail);
    void profileStateChanged(const QString& id, Longpath::KiwiSdrClient::State state,
                             const QString& detail);
    void profileTelemetryChanged(
        const QString& id,
        const Longpath::KiwiSdrReceiverTelemetry& telemetry);
    void profileWaterfallAvailabilityChanged(const QString& id,
                                             bool available,
                                             const QString& detail);
    void profileStreamReset(const QString& id);
    void sliceAssignmentChanged(int sliceId, const QString& profileId);
    // Longpath (2026-09-21): what the assigned Kiwi was last told to
    // follow -- on assignment and on every updateSliceTracking(). The
    // client itself lives on its own thread; this is the observable edge.
    void sliceTrackingUpdated(int sliceId, double frequencyMhz,
                              const QString& mode, int filterLowHz,
                              int filterHighHz, int cwPitchHz);
    void audioSourceEnabledChanged(const QString& id, bool enabled);
    // Emitted when a profile is removed entirely, so the audio engine can free
    // the per-source DSP state (disabling alone only quiesces it — #3668 review).
    void audioSourceRemoved(const QString& id);
    void decodedAudioReady(const QString& id, const QByteArray& pcm24kStereoFloat);
    void waterfallRowReady(const QString& id, const QString& panId,
                           const QVector<float>& binsDbm,
                           double lowFreqMhz, double highFreqMhz,
                           quint32 timecode);
    void waterfallDisplayRangeChanged(const QString& id, float minDbm,
                                      float maxDbm, bool autoRange);
    void meterReadingReady(
        const QString& id,
        const Longpath::KiwiSdrProtocol::MeterReading& reading);
    void profileNeedsInitialTracking(const QString& id);
    void waterfallPreviewEnabledChanged(const QString& id, bool enabled);

private:
    KiwiSdrClient* ensureClient(const QString& id);
    KiwiSdrClient* client(const QString& id) const;
    int profileIndex(const QString& id) const;
    void loadSettings();
    void saveSettings() const;
    void loadProfilePassword(const QString& id);
    void deleteProfilePassword(const QString& id);
    void queueProfilePasswordStore(const QString& id, const QString& password,
                                   bool profileRemoval);
    void setProfilePasswordPersistence(
        const QString& id, KiwiSdrPasswordPersistenceState state,
        const QString& detail = {});
    static QString profilePasswordKey(const QString& id);
    bool shouldMaintainProfileConnection(const QString& id) const;
    void ensureClientThread();
    void invokeClient(const QString& id, std::function<void(KiwiSdrClient*)> fn);
    void destroyClient(const QString& id, bool blocking = false);
    void scheduleReconnect(const QString& id);
    void scheduleWaitingReconnectIfRecommended(const QString& id);
    void cancelReconnect(const QString& id);
    static QString sanitizedName(const QString& name, const QString& endpoint);

    QVector<KiwiSdrAntennaProfile> m_profiles;
    QHash<QString, KiwiSdrClient*> m_clients;
    QHash<QString, KiwiSdrClient::State> m_states;
    QHash<QString, bool> m_clientHasTrackedSlice;
    QHash<QString, QTimer*> m_reconnectTimers;
    QHash<QString, QString> m_stateDetails;
    QHash<QString, KiwiSdrReceiverTelemetry> m_telemetry;
    QHash<QString, bool> m_waterfallAvailable;
    QHash<QString, QString> m_waterfallDetails;
    QHash<QString, KiwiSdrWaterfallDisplayRange> m_waterfallDisplayRanges;
    QHash<int, QString> m_sliceAssignments;
    QHash<QString, QString> m_profilePasswords;
    QHash<QString, quint64> m_profilePasswordRevisions;
    struct PendingPasswordStore {
        quint64 revision{0};
        QString password;
        bool profileRemoval{false};
    };
    QHash<QString, KiwiSdrPasswordPersistenceState>
        m_profilePasswordPersistenceStates;
    QHash<QString, QString> m_profilePasswordPersistenceDetails;
    QSet<QString> m_loadedProfilePasswords;
    QSet<QString> m_loadingProfilePasswords;
    QSet<QString> m_pendingPasswordConnects;
    std::shared_ptr<IKiwiSdrCredentialStore> m_credentialStore;
    QString m_operatorCallsign;
    QThread* m_clientThread{nullptr};
};

} // namespace Longpath

Q_DECLARE_METATYPE(Longpath::KiwiSdrPasswordPersistenceState)
