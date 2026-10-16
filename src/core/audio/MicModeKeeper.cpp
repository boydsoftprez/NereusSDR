// =================================================================
// src/core/audio/MicModeKeeper.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See MicModeKeeper.h.
//
// Modification history (NereusSDR):
//   2026-10-16: new. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#include "core/audio/MicModeKeeper.h"

#include <QElapsedTimer>
#include <QProcess>

#include <algorithm>
#include <cstdio>

namespace NereusSDR {

namespace {

// The helper's exit codes.  Small numbers a crash or a kill never gives.
constexpr int kExitAlreadyStandard = 0;
constexpr int kExitMadeStandard = 1;
constexpr int kExitRefused = 2;
constexpr int kExitUnavailable = 3;
constexpr int kExitNoBundle = 4;

// The helper's one line on standard output: the mode it read before.
constexpr char kBeforePrefix[] = "mic-mode-before ";

} // namespace

MicModeResult keepMicModeStandard(const QString& bundleId, const MicModeCalls& calls)
{
    MicModeResult result;
    if (bundleId.isEmpty()) {
        result.outcome = MicModeOutcome::NoBundle;
        return result;
    }
    if (!calls.read || !calls.write) {
        result.outcome = MicModeOutcome::Unavailable;
        return result;
    }
    result.before = calls.read(bundleId);
    if (result.before == kMicModeNotRead) {
        // Not read, so not known to be anything but Standard: leave it.
        result.outcome = MicModeOutcome::Refused;
        return result;
    }
    if (result.before == kMicModeStandard) {
        result.outcome = MicModeOutcome::AlreadyStandard;
        return result;
    }
    // Whatever mode it was, NereusSDR wants none of the system's voice
    // processing on its mic.  Read back: a yes that did not stick is no.
    if (!calls.write(kMicModeStandard, bundleId) || calls.read(bundleId) != kMicModeStandard) {
        result.outcome = MicModeOutcome::Refused;
        return result;
    }
    result.outcome = MicModeOutcome::MadeStandard;
    return result;
}

QString micModeName(long mode)
{
    switch (mode) {
    case kMicModeStandard:       return QStringLiteral("Standard");
    case kMicModeWideSpectrum:   return QStringLiteral("Wide Spectrum");
    case kMicModeVoiceIsolation: return QStringLiteral("Voice Isolation");
    default: break;
    }
    return QStringLiteral("mode %1").arg(mode);
}

QString micModeOutcomeText(MicModeOutcome outcome)
{
    switch (outcome) {
    case MicModeOutcome::AlreadyStandard: return QStringLiteral("Mic Mode is Standard");
    case MicModeOutcome::MadeStandard:    return QStringLiteral("Mic Mode set to Standard");
    case MicModeOutcome::Refused:         return QStringLiteral("Mic Mode could not be set to Standard");
    case MicModeOutcome::Unavailable:     return QStringLiteral("Mic Mode is not available on this system");
    case MicModeOutcome::NoBundle:        return QStringLiteral("no bundle id, so no Mic Mode");
    }
    return QStringLiteral("Mic Mode outcome unknown");
}

int micModeExitCode(MicModeOutcome outcome)
{
    switch (outcome) {
    case MicModeOutcome::AlreadyStandard: return kExitAlreadyStandard;
    case MicModeOutcome::MadeStandard:    return kExitMadeStandard;
    case MicModeOutcome::Refused:         return kExitRefused;
    case MicModeOutcome::Unavailable:     return kExitUnavailable;
    case MicModeOutcome::NoBundle:        return kExitNoBundle;
    }
    return kExitRefused;
}

bool micModeOutcomeFromExitCode(int code, MicModeOutcome* outcome)
{
    MicModeOutcome found = MicModeOutcome::Refused;
    switch (code) {
    case kExitAlreadyStandard: found = MicModeOutcome::AlreadyStandard; break;
    case kExitMadeStandard:    found = MicModeOutcome::MadeStandard; break;
    case kExitRefused:         found = MicModeOutcome::Refused; break;
    case kExitUnavailable:     found = MicModeOutcome::Unavailable; break;
    case kExitNoBundle:        found = MicModeOutcome::NoBundle; break;
    default:
        return false;
    }
    if (outcome != nullptr) {
        *outcome = found;
    }
    return true;
}

QByteArray micModeBeforeLine(long before)
{
    return QByteArray(kBeforePrefix) + QByteArray::number(qlonglong(before)) + '\n';
}

long micModeBeforeFromOutput(const QByteArray& output)
{
    for (const QByteArray& line : output.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (!trimmed.startsWith(kBeforePrefix)) {
            continue;
        }
        bool ok = false;
        const qlonglong value = trimmed.mid(int(sizeof(kBeforePrefix)) - 1).toLongLong(&ok);
        if (ok) {
            return long(value);
        }
    }
    return kMicModeNotRead;
}

int answerMicModeRequest(const QString& bundleId, const MicModeCalls& calls)
{
    const MicModeResult result = keepMicModeStandard(bundleId, calls);
    const QByteArray before = micModeBeforeLine(result.before);
    std::fwrite(before.constData(), 1, size_t(before.size()), stdout);
    std::fflush(stdout);
    std::fprintf(stderr, "nereus-audio-capture: %s\n",
                 qPrintable(micModeOutcomeText(result.outcome)));
    return micModeExitCode(result.outcome);
}

MicModeHelperAnswer runMicModeHelper(const QString& helper, const QString& bundleId, int waitMs)
{
    MicModeHelperAnswer answer;
    QElapsedTimer clock;
    clock.start();
    QProcess process;
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(helper, {QString::fromLatin1(kKeepMicModeStandardArg), bundleId});
    const bool started = process.waitForStarted(waitMs);
    const bool finished = started
        && process.waitForFinished(std::max(0, waitMs - int(clock.elapsed())));
    if (!finished) {
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(waitMs);
            answer.kind = MicModeHelperAnswer::Kind::NoAnswer;
        }
        answer.elapsedMs = clock.elapsed();
        return answer;
    }
    answer.elapsedMs = clock.elapsed();
    if (process.exitStatus() != QProcess::NormalExit
        || !micModeOutcomeFromExitCode(process.exitCode(), &answer.result.outcome)) {
        return answer;
    }
    answer.kind = MicModeHelperAnswer::Kind::Answered;
    answer.result.before = micModeBeforeFromOutput(process.readAllStandardOutput());
    return answer;
}

MicModeLogLine micModeLogLine(const MicModeHelperAnswer& answer, int waitMs)
{
    const QString took = QStringLiteral(" (%1 ms)").arg(answer.elapsedMs);
    switch (answer.kind) {
    case MicModeHelperAnswer::Kind::NoAnswer:
        return {QStringLiteral("Mic Mode: no answer from the mic helper within %1 ms; "
                               "Mic Mode left as it is").arg(waitMs), true};
    case MicModeHelperAnswer::Kind::Ended:
        return {QStringLiteral("Mic Mode: the mic helper ended without an answer; "
                               "Mic Mode left as it is") + took, true};
    case MicModeHelperAnswer::Kind::Answered:
        break;
    }
    const MicModeResult& result = answer.result;
    switch (result.outcome) {
    case MicModeOutcome::AlreadyStandard:
    case MicModeOutcome::NoBundle:
        return {};
    case MicModeOutcome::MadeStandard:
        return {QStringLiteral("Mic Mode: set to Standard for this app; it was %1")
                    .arg(micModeName(result.before)) + took, false};
    case MicModeOutcome::Refused:
        if (result.before == kMicModeNotRead) {
            return {QStringLiteral("Mic Mode: the system could not read this app's Mic Mode; "
                                   "Mic Mode left as it is") + took, true};
        }
        return {QStringLiteral("Mic Mode: the system did not take Standard for this app; "
                               "it was %1").arg(micModeName(result.before)) + took, true};
    case MicModeOutcome::Unavailable:
        return {QStringLiteral("Mic Mode: this macOS has no Mic Mode call; "
                               "Mic Mode left as it is") + took, false};
    }
    return {};
}

} // namespace NereusSDR
