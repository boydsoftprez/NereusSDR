// no-port-check: NereusSDR-original model/session orchestration. All calibration
// and plot algorithms remain in their source-backed Core adapters.
#include "PureSignalSessionFacade.h"
#include "core/AppSettings.h"
#include "core/PureSignal.h"
#include "core/TwoToneController.h"
#include "core/dsp/DspAssetService.h"
#include "core/dsp/DspAssetValidation.h"
#include "core/session/Ps3DisplayCodec.h"
#include "core/session/media/DisplayBudget.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <cmath>
#include <limits>
#include <utility>

namespace NereusSDR {
namespace {
const std::pair<const char*, int Ps3StatusSnapshot::*> kIntegers[] = {
    {"channelId", &Ps3StatusSnapshot::channelId},
    {"feedbackRateHz", &Ps3StatusSnapshot::feedbackRateHz},
    {"txMonitorDdc", &Ps3StatusSnapshot::txMonitorDdc},
    {"feedbackDdc", &Ps3StatusSnapshot::feedbackDdc},
    {"feedbackChannelId", &Ps3StatusSnapshot::feedbackChannelId},
    {"feedbackLevel", &Ps3StatusSnapshot::feedbackLevel},
    {"successfulCalibrations", &Ps3StatusSnapshot::successfulCalibrations},
    {"solutionStatusBits", &Ps3StatusSnapshot::solutionStatusBits},
    {"attemptedCalibrations", &Ps3StatusSnapshot::attemptedCalibrations},
    {"fileStatusBits", &Ps3StatusSnapshot::fileStatusBits},
    {"dogCount", &Ps3StatusSnapshot::dogCount},
    {"engineState", &Ps3StatusSnapshot::engineState},
    {"saveResult", &Ps3StatusSnapshot::saveResult},
    {"restoreResult", &Ps3StatusSnapshot::restoreResult},
};
const std::pair<const char*, bool Ps3StatusSnapshot::*> kBooleans[] = {
    {"psEnabled", &Ps3StatusSnapshot::psEnabled},
    {"mox", &Ps3StatusSnapshot::mox},
    {"pumpActive", &Ps3StatusSnapshot::pumpActive},
    {"correctionsApplied", &Ps3StatusSnapshot::correctionsApplied},
    {"solutionComparisonFailed", &Ps3StatusSnapshot::solutionComparisonFailed},
    {"overdriveOrBucketFillFailure", &Ps3StatusSnapshot::overdriveOrBucketFillFailure},
    {"saveFailed", &Ps3StatusSnapshot::saveFailed},
    {"restoreFailed", &Ps3StatusSnapshot::restoreFailed},
    {"runCalibrationProcessing", &Ps3StatusSnapshot::runCalibrationProcessing},
    {"correctionRun", &Ps3StatusSnapshot::correctionRun},
    {"correctionBusy", &Ps3StatusSnapshot::correctionBusy},
    {"savePending", &Ps3StatusSnapshot::savePending},
    {"restorePending", &Ps3StatusSnapshot::restorePending},
};
const std::pair<const char*, double Ps3StatusSnapshot::*> kDoubles[] = {
    {"requestedTxDelayNs", &Ps3StatusSnapshot::requestedTxDelayNs},
    {"appliedTxDelayNs", &Ps3StatusSnapshot::appliedTxDelayNs},
    {"hardwarePeak", &Ps3StatusSnapshot::hardwarePeak},
    {"maxTx", &Ps3StatusSnapshot::maxTx},
};
const std::pair<const char*, std::uint64_t Ps3StatusSnapshot::*> kSequences[] = {
    {"sessionGeneration", &Ps3StatusSnapshot::sessionGeneration},
    {"sequence", &Ps3StatusSnapshot::sequence},
    {"saveGeneration", &Ps3StatusSnapshot::saveGeneration},
    {"restoreGeneration", &Ps3StatusSnapshot::restoreGeneration},
    {"pairedBlocks", &Ps3StatusSnapshot::pairedBlocks},
};
QString encodeStatus(const Ps3StatusSnapshot& status)
{
    QJsonObject object;
    object.insert("schema", 1);
    object.insert("correctionSummaryValid", status.correctionSummaryValid);
    object.insert("correctionGainAtPeak", status.correctionGainAtPeak);
    object.insert("correctionPhaseSpanDegrees", status.correctionPhaseSpanDegrees);
    object.insert("pairedInputValid", status.pairedInputValid);
    object.insert("txMonitorPeak", status.txMonitorPeak);
    object.insert("feedbackPeak", status.feedbackPeak);
    for (const auto& [name, member] : kIntegers) {
        object.insert(name, status.*member);
    }
    for (const auto& [name, member] : kBooleans) {
        object.insert(name, status.*member);
    }
    for (const auto& [name, member] : kDoubles) {
        object.insert(name, status.*member);
    }
    for (const auto& [name, member] : kSequences) {
        object.insert(name, QString::number(status.*member));
    }
    object.insert("capturedAt", QString::number(status.capturedAtUnixMilliseconds));
    QJsonArray raw;
    for (int value : status.raw) {
        raw.append(value);
    }
    object.insert("raw", raw);
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
std::optional<Ps3StatusSnapshot> decodeStatus(const QString& json)
{
    if (json.size() > 8192) {
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    const QJsonObject object = document.object();
    if (!document.isObject() || object.value("schema").toInt() != 1) {
        return std::nullopt;
    }
    Ps3StatusSnapshot status;
    const auto integer = [](const QJsonValue& value) {
        const double number = value.toDouble(std::numeric_limits<double>::quiet_NaN());
        return value.isDouble() && std::isfinite(number) && std::floor(number) == number
            && number >= std::numeric_limits<int>::min() && number <= std::numeric_limits<int>::max();
    };
    for (const auto& [name, member] : kIntegers) {
        if (!integer(object.value(name))) {
            return std::nullopt;
        }
        status.*member = object.value(name).toInt();
    }
    for (const auto& [name, member] : kBooleans) {
        if (!object.value(name).isBool()) {
            return std::nullopt;
        }
        status.*member = object.value(name).toBool();
    }
    for (const auto& [name, member] : kDoubles) {
        if (!object.value(name).isDouble() || !std::isfinite(object.value(name).toDouble())) {
            return std::nullopt;
        }
        status.*member = object.value(name).toDouble();
    }
    for (const auto& [name, member] : kSequences) {
        bool ok = false;
        status.*member = object.value(name).toString().toULongLong(&ok);
        if (!ok) {
            return std::nullopt;
        }
    }
    bool timeOk = false;
    status.capturedAtUnixMilliseconds = object.value("capturedAt").toString().toLongLong(&timeOk);
    const QJsonArray raw = object.value("raw").toArray();
    if (!timeOk || raw.size() != 16) {
        return std::nullopt;
    }
    for (int i = 0; i < 16; ++i) {
        if (!integer(raw[i])) {
            return std::nullopt;
        }
        status.raw[i] = raw[i].toInt();
    }
    // Additive schema-1 telemetry: older Cores omit these fields. Never
    // substitute a configured hardware peak for an absent measurement.
    const auto finiteNumber = [&object](const char* name) {
        const QJsonValue value = object.value(name);
        return value.isDouble() && std::isfinite(value.toDouble());
    };
    if (object.value("correctionSummaryValid").toBool(false)
        && finiteNumber("correctionGainAtPeak")
        && object.value("correctionGainAtPeak").toDouble() > 0.0
        && finiteNumber("correctionPhaseSpanDegrees")
        && object.value("correctionPhaseSpanDegrees").toDouble() >= 0.0) {
        status.correctionSummaryValid = status.correctionsApplied;
        status.correctionGainAtPeak = object.value("correctionGainAtPeak").toDouble();
        status.correctionPhaseSpanDegrees = object.value("correctionPhaseSpanDegrees").toDouble();
    }
    if (object.value("pairedInputValid").toBool(false)
        && finiteNumber("txMonitorPeak") && finiteNumber("feedbackPeak")
        && object.value("txMonitorPeak").toDouble() >= 0.0
        && object.value("feedbackPeak").toDouble() >= 0.0) {
        status.pairedInputValid = true;
        status.txMonitorPeak = object.value("txMonitorPeak").toDouble();
        status.feedbackPeak = object.value("feedbackPeak").toDouble();
    }
    return status;
}
bool noArguments(const QVariantMap& arguments) { return arguments.isEmpty(); }
bool oneArgument(const QVariantMap& arguments, const QString& key, int type)
{
    return arguments.size() == 1 && arguments.contains(key)
        && arguments.value(key).metaType().id() == type;
}
}

PureSignalSessionFacade::PureSignalSessionFacade(RadioModel* radio, PureSignal* coordinator,
                                                 QObject* parent)
    : QObject(parent), m_radio(radio)
{
    m_displayTimer.setInterval(kPs3DisplayPollIntervalMs);
    connect(&m_displayTimer, &QTimer::timeout, this, &PureSignalSessionFacade::acquireDisplay);
    if (radio) {
        connect(radio, &RadioModel::pureSignalCoordinatorReady, this,
                &PureSignalSessionFacade::setCoordinator);
        connect(radio, &RadioModel::connectionStateChanged, this, [this]() { refreshStatus(); });
        // canActuate depends on the station's receive-only policy; follow a
        // change on the same call, not on the coordinator's next poll.
        connect(radio, &RadioModel::receiveOnlyStationPolicyChanged, this,
                [this]() { refreshStatus(); });
        if (radio->twoToneController()) {
            connect(radio->twoToneController(), &TwoToneController::twoToneActiveChanged,
                    this, [this]() { refreshStatus(); });
        }
        // R-R3-49 (parity Task 7): in a window, arming waits while the
        // Core's radio is on the air; the controls follow it at once.
        connect(radio, &RadioModel::coreOnAirChanged, this, [this]() {
            if (remote()) {
                emit statusChanged();
            }
        });
    }
    setCoordinator(coordinator ? coordinator : radio ? radio->pureSignal() : nullptr);
}

void PureSignalSessionFacade::followTwoToneController()
{
    if (!m_radio || !m_radio->twoToneController()) {
        return;
    }
    connect(m_radio->twoToneController(), &TwoToneController::twoToneActiveChanged,
            this, &PureSignalSessionFacade::refreshStatus, Qt::UniqueConnection);
}

PureSignalSessionFacade::~PureSignalSessionFacade()
{
    if (remote() && m_ampViewSubscribed) {
        emit displaySubscriptionRequested(false);
    }
    // In-flight files stay in station staging. The native worker belongs to
    // TxChannel and can outlive this facade/coordinator; the store removes
    // abandoned staging files on the next process start after native teardown.

}

bool PureSignalSessionFacade::remote() const
{
    return m_radio && m_radio->role() == RadioModel::Role::Remote;
}

bool PureSignalSessionFacade::isArmingAction(Ps3Action action)
{
    return action == Ps3Action::Single || action == Ps3Action::StartAutomatic
        || action == Ps3Action::ApplyCurrentCorrection || action == Ps3Action::RestoreCorrection;
}

bool PureSignalSessionFacade::canArm() const
{
    if (!remote()) {
        return m_canActuate;
    }
    if (!m_available || !m_remoteRuntimeCanActuate) {
        return false;
    }
    return m_remoteTxPermitted || (m_remoteArmingOffered && !m_radio->isCoreOnAir());
}

QString PureSignalSessionFacade::armingRefusal() const
{
    // Fix wave GUI-I7: a reason whenever canArm() is false, so no control
    // that follows canArm greys without one.
    if (canArm()) {
        return {};
    }
    if (!remote() || !m_available || !m_remoteRuntimeCanActuate) {
        // A station: its PureSignal needs a connected radio that has it
        // (RadioModel's readiness predicate). A window: its Core's does.
        return needsRadioReason();
    }
    if (!m_remoteArmingOffered) {
        // A Core below transmitSettingsVersion 7 arms only for a window
        // that may transmit, as requestAction says.
        return QStringLiteral("PureSignal cannot be run from a remote window.");
    }
    if (m_radio->isCoreOnAir()) {
        return RadioModel::onAirReason();
    }
    return needsRadioReason();
}

QString PureSignalSessionFacade::twoToneRefusal() const
{
    // Fix wave GUI-I7: the two-tone test keys the radio, so it follows
    // canActuate(); this says why when canActuate() is false.
    if (m_canActuate) {
        return {};
    }
    if (!remote() || !m_available || !m_remoteRuntimeCanActuate) {
        return needsRadioReason();
    }
    return QStringLiteral("The 2-tone test needs permission to transmit from the Core.");
}

QString PureSignalSessionFacade::needsRadioReason()
{
    return QStringLiteral("PureSignal needs a connected radio that supports it.");
}

QString PureSignalSessionFacade::settingsRefusal() const
{
    if (remote() && m_remoteArmingOffered && m_radio->isCoreOnAir()) {
        return RadioModel::onAirReason();
    }
    return {};
}
PureSignalSettings* PureSignalSessionFacade::settings() const
{
    return m_radio ? m_radio->pureSignalSettings() : m_coordinator ? m_coordinator->settings() : nullptr;
}
void PureSignalSessionFacade::setCoordinator(PureSignal* coordinator)
{
    if (m_coordinator == coordinator) {
        refreshStatus();
        return;
    }
    if (m_coordinator) {
        disconnect(m_coordinator, nullptr, this, nullptr);
    }
    resetSession();
    m_coordinator = coordinator;
    if (coordinator) {
        coordinator->setSessionGeneration(m_displayGeneration);
        connect(coordinator, &PureSignal::ps3StatusChanged, this, &PureSignalSessionFacade::refreshStatus);
        connect(coordinator, &PureSignal::fileOperationCompleted, this, &PureSignalSessionFacade::finishFile);
        connect(coordinator, &PureSignal::fileOperationRetired, this,
                [this](int kind, quint64 generation, quint64 completion) {
            const auto ids = m_pending.keys();
            for (quint32 id : ids) {
                const PendingOperation pending = m_pending.value(id);
                if (pending.file && static_cast<int>(pending.file->kind) == kind
                    && pending.file->sessionGeneration == generation
                    && pending.file->nativeCompletionGeneration == completion) {
                    retainRetiredSave(pending);
                    m_pending.remove(id);
                    if (pending.generation == m_actionGeneration) {
                        finishOperation(id, Ps3ActionPhase::Failed,
                            QStringLiteral("The connection to the Core changed, so this file operation stopped."));
                    }
                }
            }
        });
        connect(coordinator, &QObject::destroyed, this, [this]() {
            m_coordinator = nullptr;
            resetSession();
            refreshStatus();
        });
    }
    refreshStatus();
    reconcileDisplayTimer();
}
QByteArray PureSignalSessionFacade::actionVerb(Ps3Action action)
{
    switch (action) {
    case Ps3Action::OffReset: return "ps3.off";
    case Ps3Action::Single: return "ps3.single";
    case Ps3Action::StartAutomatic: return "ps3.automatic";
    case Ps3Action::ApplyCurrentCorrection: return "ps3.applyCurrent";
    case Ps3Action::SetTwoTone: return "ps3.twoTone";
    case Ps3Action::SaveCorrection: return "ps3.saveCorrection";
    case Ps3Action::RestoreCorrection: return "ps3.restoreCorrection";
    }
    return {};
}
std::optional<Ps3Action> PureSignalSessionFacade::actionForVerb(const QByteArray& verb)
{
    for (const Ps3Action action : {Ps3Action::OffReset, Ps3Action::Single,
        Ps3Action::StartAutomatic, Ps3Action::ApplyCurrentCorrection, Ps3Action::SetTwoTone,
        Ps3Action::SaveCorrection, Ps3Action::RestoreCorrection}) {
        if (actionVerb(action) == verb) {
            return action;
        }
    }
    return std::nullopt;
}

quint32 PureSignalSessionFacade::requestAction(Ps3Action action, const QVariantMap& arguments)
{
    if (m_pending.size() + m_queued.size() >= 128) {
        m_lastError = QStringLiteral("Too many PureSignal requests are pending.");
        emit statusChanged();
        return 0;
    }
    if (remote()) {
        if (!m_available || !m_remoteRequest) {
            m_lastError = QStringLiteral("This Core does not offer PureSignal controls to this app.");
            emit statusChanged();
            return 0;
        }
        const bool stop = action == Ps3Action::OffReset
            || (action == Ps3Action::SetTwoTone && oneArgument(arguments, "enabled", QMetaType::Bool)
                && !arguments.value("enabled").toBool());
        // R-R3-49 (parity Task 7): arming follows canArm(); the two-tone
        // test stays with remote transmit.
        const bool permitted = isArmingAction(action) ? canArm() : m_canActuate;
        if (!stop && action != Ps3Action::SaveCorrection && !permitted) {
            const QString refusal = isArmingAction(action) ? armingRefusal() : QString();
            m_lastError = refusal.isEmpty()
                ? QStringLiteral("PureSignal cannot be run from a remote window.") : refusal;
            emit statusChanged();
            return 0;
        }
        const quint32 id = m_remoteRequest(action, arguments);
        if (id) {
            m_pending.insert(id, PendingOperation{action, m_actionGeneration});
        }
        return id;
    }
    while (m_nextOperationId == 0 || m_pending.contains(m_nextOperationId)
           || m_queued.contains(m_nextOperationId)) {
        ++m_nextOperationId;
    }
    const quint32 id = m_nextOperationId++;
    m_queued.insert(id);
    const quint64 generation = m_actionGeneration;
    QMetaObject::invokeMethod(this, [this, id, action, arguments, generation]() {
        if (generation != m_actionGeneration) {
            return;
        }
        m_queued.remove(id);
        const Ps3ActionResult result = executeAction(action, arguments, id);
        finishOperation(id, result.phase, result.reason, result.values);
    }, Qt::QueuedConnection);
    return id;
}

bool PureSignalSessionFacade::argumentsFit(Ps3Action action, const QVariantMap& arguments)
{
    if (action == Ps3Action::SetTwoTone) {
        return oneArgument(arguments, "enabled", QMetaType::Bool);
    }
    if (action == Ps3Action::SaveCorrection) {
        return oneArgument(arguments, "label", QMetaType::QString)
            && !arguments.value("label").toString().trimmed().isEmpty()
            && arguments.value("label").toString().size() <= 128;
    }
    if (action == Ps3Action::RestoreCorrection) {
        return oneArgument(arguments, "assetId", QMetaType::QString)
            && arguments.value("assetId").toString().size() <= 80;
    }
    return noArguments(arguments);
}

Ps3ActionResult PureSignalSessionFacade::executeAction(Ps3Action action,
                                                       const QVariantMap& arguments,
                                                       quint32 operationId)
{
    const auto fail = [operationId](const QString& reason) {
        return Ps3ActionResult{operationId, Ps3ActionPhase::Failed, reason, {}};
    };
    const bool valid = argumentsFit(action, arguments);
    if (!valid || operationId == 0 || m_pending.contains(operationId) || m_queued.contains(operationId)
        || m_pending.size() >= 128) {
        return fail(QStringLiteral("The Core could not read this PureSignal request."));
    }
    const bool stop = action == Ps3Action::OffReset
        || (action == Ps3Action::SetTwoTone && !arguments.value("enabled").toBool());
    if (!stop && action != Ps3Action::SetTwoTone && action != Ps3Action::SaveCorrection) {
        for (const PendingOperation& previous : std::as_const(m_pending)) {
            if (previous.action != Ps3Action::SetTwoTone
                && previous.action != Ps3Action::SaveCorrection
                && !(action == Ps3Action::StartAutomatic
                    && previous.action == Ps3Action::Single)) {
                return fail(QStringLiteral("A PureSignal operation is still pending. "
                                           "Wait for completion or use Off to cancel it."));
            }
        }
    }
    if (!m_coordinator) {
        return stop ? Ps3ActionResult{operationId, Ps3ActionPhase::Completed, {}, {}}
                    : fail(QStringLiteral("PureSignal is unavailable until the radio is ready."));
    }
    if (!stop && action != Ps3Action::SaveCorrection && !m_coordinator->canActuate()) {
        return fail(QStringLiteral("PureSignal cannot start in the radio's current state."));
    }
    if ((action == Ps3Action::Single || action == Ps3Action::StartAutomatic)
        && (!settings() || !settings()->runCalibrationProcessing())) {
        return fail(QStringLiteral("Calibration processing is paused. Enable Run calibration processing before starting calibration."));
    }
    PendingOperation pending{action, m_actionGeneration};
    pending.startAttempts = m_status.attemptedCalibrations;
    pending.startSuccesses = m_status.successfulCalibrations;
    if (action == Ps3Action::SaveCorrection || action == Ps3Action::RestoreCorrection) {
        DspAssetStore* store = m_radio ? m_radio->dspAssets()->store() : nullptr;
        if (!store || !store->isValid()) {
            return fail(QStringLiteral("The Core cannot store PureSignal corrections right now."));
        }
        QString reason;
        if (action == Ps3Action::SaveCorrection) {
            if (!m_coordinator->correctionsBeingApplied()) {
                return fail(QStringLiteral("There is no current correction to save."));
            }
            pending.path = store->rootDirectory() + "/staging/ps-"
                + QUuid::createUuid().toString(QUuid::Id128) + ".tmp";
            pending.label = arguments.value("label").toString();
        } else {
            pending.path = store->resolvePath(arguments.value("assetId").toString(),
                DspAssetKind::Ps3Correction, m_radio->currentRadioMac(), &reason);
        }
        if (pending.path.isEmpty() || !DspAssetValidation::validateEncodedPath(pending.path, 256, &reason)) {
            return fail(reason);
        }
        pending.file = action == Ps3Action::SaveCorrection
            ? m_coordinator->beginSaveCorrections(pending.path)
            : m_coordinator->beginRestoreCorrections(pending.path);
        if (!pending.file) {
            return fail(QStringLiteral("PureSignal could not save or load the correction, or another one is still in progress."));
        }
        m_pending.insert(operationId, pending);
        return {operationId, Ps3ActionPhase::Pending, {}, {}};
    }
    switch (action) {
    case Ps3Action::OffReset:
        m_coordinator->reset();
        for (quint32 id : m_pending.keys()) {
            const PendingOperation previous = m_pending.value(id);
            retainRetiredSave(previous);
            finishOperation(id, Ps3ActionPhase::Failed,
                            QStringLiteral("PureSignal was turned off, so this action stopped."));
        }
        break;
    case Ps3Action::Single:
        m_coordinator->singleCalibrate();
        break;
    case Ps3Action::StartAutomatic:
        m_coordinator->setAutoCalEnabled(true);
        // A restored desired preference can already be true while operation
        // is stopped. This explicit action must start it even without a
        // settings valueChanged signal; hydration never calls this path.
        if (!m_coordinator->applyAcceptedSettingsToEngine()
            || !m_coordinator->resumeAutomaticCalibrationPreference()) {
            return fail(QStringLiteral("Automatic calibration could not start in the current radio state."));
        }
        // A Single request may wait indefinitely for a full amplitude sweep.
        // The explicit Auto mode change replaces that request; it must not
        // depend on Single producing a correction first.
        for (quint32 id : m_pending.keys()) {
            if (m_pending.value(id).action == Ps3Action::Single) {
                finishOperation(id, Ps3ActionPhase::Failed,
                    QStringLiteral("Automatic calibration replaced the single calibration request."));
            }
        }
        break;
    case Ps3Action::ApplyCurrentCorrection:
        if (!m_coordinator->applyCurrentCorrection()) {
            return fail(QStringLiteral("There is no current correction available to apply."));
        }
        break;
    case Ps3Action::SetTwoTone: {
        pending.twoToneTarget = arguments.value("enabled").toBool();
        TwoToneController* controller = m_radio ? m_radio->twoToneController() : nullptr;
        if (pending.twoToneTarget && controller && controller->isDeactivationInFlight()) {
            return fail(QStringLiteral("The two-tone test is still stopping. Try again after it stops."));
        }
        for (quint32 id : m_pending.keys()) {
            const PendingOperation previous = m_pending.value(id);
            if (previous.action == Ps3Action::SetTwoTone
                && previous.twoToneTarget != pending.twoToneTarget) {
                finishOperation(id, Ps3ActionPhase::Failed,
                    QStringLiteral("A newer two-tone request replaced this request."));
            }
        }
        m_coordinator->setTwoToneOn(pending.twoToneTarget);
        refreshStatus();
        if (m_twoToneOn == pending.twoToneTarget
            && (!controller || (!controller->isActivationInFlight()
                && !controller->isDeactivationInFlight()))) {
            return {operationId, Ps3ActionPhase::Completed, {}, {}};
        }
        if (pending.twoToneTarget && (!controller || !controller->isActivationInFlight())) {
            return fail(QStringLiteral("The two-tone test did not change."));
        }
        // Start and stop can include the controller's MOX/TUNE settle wait.
        // Its authoritative state change completes the request after that wait.
        m_pending.insert(operationId, pending);
        return {operationId, Ps3ActionPhase::Pending, {}, {}};
    }
    case Ps3Action::SaveCorrection:
    case Ps3Action::RestoreCorrection:
        break;
    }
    m_pending.insert(operationId, pending);
    // Completion follows the coordinator's next acknowledged status, not the
    // return from its asynchronous command setter.
    return {operationId, Ps3ActionPhase::Pending, {}, {}};
}

void PureSignalSessionFacade::finishOperation(quint32 operationId, Ps3ActionPhase phase,
                                               const QString& reason, const QVariantMap& values)
{
    if (phase == Ps3ActionPhase::Completed || phase == Ps3ActionPhase::Failed) {
        m_pending.remove(operationId);
    }
    if (m_lastError != reason) {
        m_lastError = reason;
        emit statusChanged();
    }
    emit actionResult(operationId, phase, reason, values);
}

void PureSignalSessionFacade::retainRetiredSave(const PendingOperation& pending)
{
    if (pending.action != Ps3Action::SaveCorrection || pending.path.isEmpty()) {
        return;
    }
    if (m_coordinator) {
        m_retiredSaves.append({m_coordinator, pending});
    }
}

void PureSignalSessionFacade::finishFile(int kind, int result, quint64 generation, quint64 completion)
{
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        const PendingOperation pending = it.value();
        if (!pending.file || static_cast<int>(pending.file->kind) != kind
            || pending.file->sessionGeneration != generation
            || pending.file->nativeCompletionGeneration != completion) {
            continue;
        }
        const quint32 id = it.key();
        m_pending.erase(it);
        if (pending.generation != m_actionGeneration) {
            if (pending.action == Ps3Action::SaveCorrection) {
                QFile::remove(pending.path);
            }
            return;
        }
        QString reason;
        QVariantMap values;
        bool success = result == static_cast<int>(Ps3FileOperationResult::Success);
        if (success && pending.action == Ps3Action::SaveCorrection) {
            QFile file(pending.path);
            DspAssetStore* store = m_radio ? m_radio->dspAssets()->store() : nullptr;
            if (!store || !file.open(QIODevice::ReadOnly)
                || file.size() > DspAssetValidation::kMaxPs3CorrectionBytes) {
                success = false;
                reason = QStringLiteral("The saved correction was too large to read back.");
            } else {
                const auto imported = store->importBytes(DspAssetKind::Ps3Correction,
                    pending.label, file.readAll(), m_radio->currentRadioMac());
                success = imported.accepted;
                reason = imported.error;
                if (success) {
                    values.insert("assetId", imported.record.id);
                }
            }
        }
        if (pending.action == Ps3Action::SaveCorrection) {
            QFile::remove(pending.path);
        }
        if (!success && reason.isEmpty()) {
            reason = QStringLiteral("PureSignal could not finish saving or loading the correction.");
        }
        finishOperation(id, success ? Ps3ActionPhase::Completed : Ps3ActionPhase::Failed, reason, values);
        return;
    }
}

void PureSignalSessionFacade::refreshStatus()
{
    if (remote()) {
        return;
    }
    m_available = m_coordinator != nullptr;
    m_canActuate = m_coordinator && m_coordinator->canActuate();
    m_twoToneOn = m_radio && m_radio->twoToneController() && m_radio->twoToneController()->isActive();
    m_status = m_coordinator ? m_coordinator->ps3StatusSnapshot() : Ps3StatusSnapshot{};
    if (m_radio && m_coordinator) {
        const Ps3RoutingSnapshot route = m_radio->pureSignalRoutingSnapshot();
        m_status.txMonitorDdc = route.txMonitorDdc;
        m_status.feedbackDdc = route.feedbackDdc;
        m_status.feedbackChannelId = route.feedbackChannelId;
        m_status.pumpActive = route.pumpActive;
        m_status.pairedBlocks = route.pairedBlocks;
        m_status.pairedInputValid = route.pairedInputValid;
        m_status.txMonitorPeak = route.txMonitorPeak;
        m_status.feedbackPeak = route.feedbackPeak;
    }
    for (auto it = m_retiredSaves.begin(); it != m_retiredSaves.end();) {
        const bool completed = it->owner && it->owner == m_coordinator
            && !m_status.savePending && it->operation.file
            && m_status.saveGeneration >= it->operation.file->nativeCompletionGeneration;
        if (completed) {
            QFile::remove(it->operation.path);
            it = m_retiredSaves.erase(it);
        } else if (!it->owner) {
            // No live completion readback remains; leave staging cleanup to
            // the next process start instead of racing a channel worker.
            it = m_retiredSaves.erase(it);
        } else {
            ++it;
        }
    }
    m_statusJson = encodeStatus(m_status);
    emit statusChanged();
    if (!m_coordinator) {
        return;
    }
    const QList<quint32> operations = m_pending.keys();
    for (quint32 id : operations) {
        const auto it = m_pending.constFind(id);
        if (it == m_pending.cend() || it->file || it->generation != m_actionGeneration) {
            continue;
        }
        bool complete = false;
        bool success = true;
        switch (it->action) {
        case Ps3Action::OffReset:
            complete = !m_status.correctionRun && !m_status.correctionBusy && !m_coordinator->isPsEnabled();
            break;
        case Ps3Action::Single:
            // WDSP 2.10 calcc.c LCALC publishes binfo[0..7] together only
            // after calcdone, so attempts and successes belong to one result.
            complete = m_status.attemptedCalibrations != it->startAttempts;
            success = m_status.successfulCalibrations != it->startSuccesses;
            break;
        case Ps3Action::StartAutomatic:
            complete = m_coordinator->isPsEnabled();
            break;
        case Ps3Action::ApplyCurrentCorrection:
            complete = m_status.correctionRun;
            break;
        case Ps3Action::SetTwoTone: {
            TwoToneController* controller = m_radio ? m_radio->twoToneController() : nullptr;
            complete = m_twoToneOn == it->twoToneTarget
                && (!controller || (!controller->isActivationInFlight()
                    && !controller->isDeactivationInFlight()));
            if (!complete && it->twoToneTarget
                && (!controller || !controller->isActivationInFlight())) {
                complete = true;
                success = false;
            }
            break;
        }
        default:
            break;
        }
        if (complete) {
            const QString reason = success ? QString()
                : it->action == Ps3Action::SetTwoTone
                    ? QStringLiteral("The two-tone test did not change.")
                    : QStringLiteral("The calibration attempt did not produce a successful correction.");
            finishOperation(id, success ? Ps3ActionPhase::Completed : Ps3ActionPhase::Failed,
                reason);
        }
    }
}

void PureSignalSessionFacade::setRemoteRequestHandler(RemoteRequestHandler handler)
{
    m_remoteRequest = std::move(handler);
}
void PureSignalSessionFacade::setRemoteCapabilities(bool available, bool canActuate,
                                                    bool armingOffered)
{
    if (!remote()) {
        return;
    }
    m_remoteCapabilityAvailable = available;
    m_remoteTxPermitted = canActuate;
    m_remoteArmingOffered = armingOffered;
    m_available = available && m_remoteRuntimeAvailable;
    m_canActuate = m_available && canActuate && m_remoteRuntimeCanActuate;
    emit statusChanged();
    if (available && m_ampViewSubscribed) {
        emit displaySubscriptionRequested(true);
    }
}
void PureSignalSessionFacade::receiveRemoteActionResult(quint32 id, const QByteArray& verb, Ps3ActionPhase phase,
                                                        const QString& reason, const QVariantMap& values)
{
    const auto it = m_pending.constFind(id);
    if (!remote() || it == m_pending.cend() || it->generation != m_actionGeneration
        || actionVerb(it->action) != verb) {
        return;
    }
    finishOperation(id, phase, reason, values);
}
bool PureSignalSessionFacade::applyRemoteProperty(const QByteArray& property, const QVariant& value)
{
    if (!remote()) {
        return false;
    }
    if (property == "statusJson") {
        const auto status = decodeStatus(value.toString());
        if (!status) {
            return false;
        }
        m_status = *status;
        m_statusJson = value.toString();
    } else if (property == "available") {
        m_remoteRuntimeAvailable = value.toBool();
        m_available = m_remoteCapabilityAvailable && m_remoteRuntimeAvailable;
        m_canActuate = m_available && m_remoteTxPermitted && m_remoteRuntimeCanActuate;
    } else if (property == "canActuate") {
        m_remoteRuntimeCanActuate = value.toBool();
        m_canActuate = m_available && m_remoteTxPermitted && m_remoteRuntimeCanActuate;
    } else if (property == "twoToneOn") {
        m_twoToneOn = value.toBool();
    } else if (property == "lastActionError") {
        m_lastError = value.toString();
    } else if (property == "displayGeneration") {
        const quint64 generation = value.toULongLong();
        if (generation == 0) {
            return false;
        }
        if (generation != m_displayGeneration) {
            m_displayGeneration = generation;
            m_display.reset();
            emit displayInvalidated();
        }
    } else {
        return false;
    }
    emit statusChanged();
    return true;
}
void PureSignalSessionFacade::resetSession()
{
    ++m_actionGeneration;
    ++m_displayGeneration;
    m_displaySequence = 0;
    m_display.reset();
    const bool remoteDisplayWasSubscribed = m_remoteAmpViewSubscribed;
    m_remoteAmpViewSubscribed = false;
    // Keep a save's path until its native completion or coordinator teardown.
    // Retiring a session must not race the writer or publish its old result.
    for (const auto& pending : std::as_const(m_pending)) {
        retainRetiredSave(pending);
    }
    m_pending.clear();
    m_queued.clear();
    if (m_coordinator) {
        m_coordinator->retireSessionOperations();
        m_coordinator->setSessionGeneration(m_displayGeneration);
    }
    if (remote()) {
        m_available = false;
        m_canActuate = false;
        m_remoteCapabilityAvailable = false;
        m_remoteTxPermitted = false;
        m_remoteArmingOffered = false;
        m_remoteRuntimeAvailable = false;
        m_remoteRuntimeCanActuate = false;
        m_twoToneOn = false;
        m_status = {};
        m_statusJson.clear();
        m_lastError.clear();
        m_remoteRequest = {};
    }
    reconcileDisplayTimer();
    const QPointer<PureSignalSessionFacade> self(this);
    emit displayInvalidated();
    if (!self) { return; }
    if (remoteDisplayWasSubscribed) {
        emit remoteAmpViewSubscriptionChanged(false);
        if (!self) { return; }
    }
    emit statusChanged();
}
void PureSignalSessionFacade::setAmpViewSubscribed(bool subscribed)
{
    if (m_ampViewSubscribed == subscribed) {
        return;
    }
    m_ampViewSubscribed = subscribed;
    if (remote()) {
        emit displaySubscriptionRequested(subscribed);
    }
    reconcileDisplayTimer();
}
void PureSignalSessionFacade::setRemoteAmpViewSubscribed(bool subscribed)
{
    if (m_remoteAmpViewSubscribed == subscribed) {
        return;
    }
    m_remoteAmpViewSubscribed = subscribed;
    reconcileDisplayTimer();
    emit remoteAmpViewSubscriptionChanged(subscribed);
}
void PureSignalSessionFacade::reconcileDisplayTimer()
{
    if (m_coordinator) {
        m_coordinator->setAmpViewSubscribed(m_ampViewSubscribed || m_remoteAmpViewSubscribed);
    }
    if (!remote() && m_coordinator && (m_ampViewSubscribed || m_remoteAmpViewSubscribed)) {
        m_displayTimer.start();
    } else {
        m_displayTimer.stop();
    }
}
void PureSignalSessionFacade::acquireDisplay()
{
    if (!m_coordinator) {
        return;
    }
    const auto snapshot = m_coordinator->ps3DisplaySnapshot(m_displayGeneration,
        ++m_displaySequence, QDateTime::currentMSecsSinceEpoch());
    if (snapshot && (!m_display || snapshot->sequence > m_display->sequence)) {
        m_display = *snapshot;
        emit displaySnapshotReady(*snapshot);
    }
}
void PureSignalSessionFacade::receiveDisplaySnapshot(const Ps3Snapshot& snapshot)
{
    if (!remote() || !m_ampViewSubscribed || snapshot.sessionGeneration != m_displayGeneration
        || (m_display && snapshot.sequence <= m_display->sequence)
        || !Ps3DisplayCodec::validateSnapshot(snapshot)) {
        return;
    }
    m_display = snapshot;
    emit displaySnapshotReady(snapshot);
}
} // namespace NereusSDR
