// Ported from AetherSDR src/core/RigctlProtocol.cpp [@1e0718ad].
// AetherSDR project attribution: Jeremy (KK7GWY), primary author, and contributors.
// https://github.com/ten9876/AetherSDR — GNU GPL v3, project LICENSE applies.
// Upstream source has no top-of-file GPL header; no notice is fabricated.
// Modification history (NereusSDR):
// 2026-10-04 - Prepare all combined mode/passband numbers before frequency effects.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - Keep prefixed session-owned PTT OFF independent of RX admission,
//              by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - Supported dispatch/response contracts adapted by J.J. Boyd
//              (KG4VCF), AI-assisted via OpenAI Codex. Native model operations,
//              frozen bindings and shared claims replace Flex command strings.
#include "RigctlProtocol.h"
#include "CatModelAdapter.h"
#include "CatTxCoordinator.h"
#include "CatService.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/TxSliceArbiter.h"
#include <QMap>
#include <QLocale>
#include <cmath>
#include <cctype>
#include <limits>
#include <optional>
namespace NereusSDR {
namespace {
// From Hamlib 4.7.0 include/hamlib/rig.h rig_errcode_e (negative wire returns).
// https://raw.githubusercontent.com/Hamlib/Hamlib/4.7.0/include/hamlib/rig.h
constexpr int kInvalid = -1;
constexpr int kRejected = -9;
constexpr int kUnavailable = -11;
constexpr int kTargetUnavailable = -12;
constexpr int kVfoInvalid = -16;
constexpr int kAccessDenied = -22;
// Native wire representability policy: largest exactly representable integral
// IEEE754 double. This is not an advertised hardware frequency range.
constexpr double kMaximumIntegralFrequency = 9007199254740991.0;
QString rprt(int code) { return QStringLiteral("RPRT %1\n").arg(code); }
// From AetherSDR src/core/RigctlProtocol.cpp:616-665,681-691 [@1e0718ad].
QString commandName(QChar command)
{
    // Short-form character assignments from Hamlib tests/rigctl_parse.c (master).
    // [original inline comment from RigctlProtocol.cpp:620]
    static const QMap<QChar, QString> names{
        // Frequency / mode
        {'f',"get_freq"},{'F',"set_freq"},{'m',"get_mode"},{'M',"set_mode"},
        // VFO
        {'v',"get_vfo"},{'V',"set_vfo"},
        // PTT
        {'t',"get_ptt"},{'T',"set_ptt"},
        // Split
        {'s',"get_split_vfo"},{'S',"set_split_vfo"},{'i',"get_split_freq"},{'I',"set_split_freq"},
        {'x',"get_split_mode"},{'X',"set_split_mode"},
        // get_split_freq_mode
        // [original inline comment from RigctlProtocol.cpp:640]
        {'k',"get_split_freq_mode"},
        // set_split_freq_mode
        // [original inline comment from RigctlProtocol.cpp:651]
        {'K',"set_split_freq_mode"},
        // Level / func / parm
        {'l',"get_level"},{'L',"set_level"},{'u',"get_func"},{'U',"set_func"},
        // get_parm — not supported
        // [original inline comment from RigctlProtocol.cpp:664]
        {'p',"get_parm"},
        // set_parm — silently accept
        // [original inline comment from RigctlProtocol.cpp:665]
        // Nereus correction: unavailable setter returns -11, without fake success.
        {'P',"set_parm"},
        // RIT / XIT
        {'j',"get_rit"},{'J',"set_rit"},{'z',"get_xit"},{'Z',"set_xit"},
        // Tuning step
        // get_ts
        // [original inline comment from RigctlProtocol.cpp:690]
        {'n',"get_ts"},
        // set_ts
        // [original inline comment from RigctlProtocol.cpp:691]
        {'N',"set_ts"},{'_',"get_info"}
    };
    return names.value(command);
}
// Native mapping to actual Nereus modes; no lossy Flex CW/RTTY fallback.
// Wire mode tokens: https://hamlib.sourceforge.net/html/rigctld.1.html (M/m).
const QMap<QString, DSPMode> kModes{
    {"LSB",DSPMode::LSB},{"USB",DSPMode::USB},{"CW",DSPMode::CWU},{"CWR",DSPMode::CWL},
    {"AM",DSPMode::AM},{"AMS",DSPMode::SAM},{"FM",DSPMode::FM},
    {"PKTLSB",DSPMode::DIGL},{"PKTUSB",DSPMode::DIGU},{"DSB",DSPMode::DSB}
};
QString modeName(DSPMode mode) { return kModes.key(mode); }
bool vfoName(const QString& name) {
    return QStringList{"VFOA","VFOB","MAIN","SUB","CURRVFO","VFO","RX","TX","MEM","VFOMEM","VFOC"}.contains(name.toUpper());
}
}
RigctlProtocol::RigctlProtocol(CatModelAdapter& adapter, CatTxCoordinator& coordinator, int channel, quint64 id)
    : m_adapter(adapter), m_coordinator(coordinator), m_model(&adapter.radioModel()),
      m_service(adapter.radioModel().catService()), m_binding(m_service->channelConfig(channel).binding), m_sessionId(id)
{}
bool RigctlProtocol::live() const
{
    // Retained protocol/session may outlive synchronous service/model destruction.
    return m_model && m_service && m_service->isStarted() && m_service->session(m_sessionId)
        && m_service->session(m_sessionId)->dialect() == CatWireDialect::Rigctld;
}
int RigctlProtocol::writeError(CatVfo vfo, const QByteArray& property) const
{
    if (!live() || !m_adapter.resolveSlice(m_binding,vfo)) { return kTargetUnavailable; }
    if (m_adapter.mayChange(m_binding,vfo,property)) { return 0; }
    // Authority refusal is distinct from a station on-air transition refusal.
    const SliceModel* slice = m_adapter.resolveSlice(m_binding,vfo);
    if (m_model->moxController()->isMox() && m_model->txBoundSlice() == slice
        && m_model->moxController()->currentKeyer().isStation()) { return kRejected; }
    return kAccessDenied;
}
QString RigctlProtocol::handleLine(const QString& line)
{
    if (!live()) { return {}; }
    QString command = line.trimmed();
    if (command.isEmpty()) { return {}; }
    // From AetherSDR src/core/RigctlProtocol.cpp:373-400 [@1e0718ad], corrected
    // against https://hamlib.sourceforge.net/html/rigctld.1.html PROTOCOL.
    // Check for extended mode prefix
    // [original inline comment from RigctlProtocol.cpp:373]
    bool extended = false; QChar separator('\n');
    if ((command.front().unicode() < 128 && std::ispunct(static_cast<unsigned char>(command.front().toLatin1()))) && command.front() != '\\' && command.front() != '?' && command.front() != '_' && command.front() != '#') {
        extended = true; separator = command.front() == '+' ? QChar('\n') : command.front(); command.remove(0,1);
    }
    // Pipe separator mode: '|' splits commands and implies extended responses
    // joined by '|' instead of newlines (standard rigctld wire protocol).
    // [original inline comment from RigctlProtocol.cpp:381-382]
    // Nereus correction: pipe is a response separator, never a command splitter;
    // ERP state is command-local, so a subsequent bare command stays bare.
    QString name, args;
    // Long form: \command_name [args]
    // [original inline comment from RigctlProtocol.cpp:419]
    if (command.startsWith('\\')) {
        const QString rest = command.mid(1); const qsizetype space = rest.indexOf(' ');
        name = space < 0 ? rest : rest.left(space); args = space < 0 ? QString() : rest.mid(space+1).trimmed();
    } else if (!command.isEmpty()) {
        // Short form: single character + optional args
        // [original inline comment from RigctlProtocol.cpp:616]
        name = commandName(command.front()); args = command.mid(1).trimmed();
    }
    QStringList parts = args.split(' ',Qt::SkipEmptyParts);
    QList<QPair<QString,QString>> values;
    const auto value = [&values](const QString& key,const QString& data) { values.append({key,data}); };
    const auto reply = [&](int error) {
        if (!live()) { return QString(); }
        QString result;
        if (extended) {
            result = (name.isEmpty() ? command : name) + ':' + (args.isEmpty() ? QString() : ' '+args) + separator;
            if (error == 0) { for (const auto& entry : values) { result += entry.first + ": " + entry.second + separator; } }
            // Each response ends with '\n'; strip trailing newline before joining
            // [original inline comment from RigctlProtocol.cpp:392]
            // Replace interior newlines with '|' for pipe-mode formatting
            // [original inline comment from RigctlProtocol.cpp:395]
            // Nereus builds records with the selected separator directly, preserving
            // the final RPRT newline without batching or persistent ERP state.
            return result + rprt(error);
        }
        if (error || values.isEmpty()) { return rprt(error); }
        for (const auto& entry : values) { result += entry.second + '\n'; }
        return result;
    };
    const auto resolveVfo = [&](const QString& token, CatVfo& vfo) {
        const QString upper = token.toUpper();
        if (upper == "VFOA" || upper == "MAIN" || upper == "RX") { vfo = CatVfo::Primary; return true; }
        if (upper == "VFOB" || upper == "SUB") { vfo = CatVfo::Secondary; return true; }
        if (upper == "VFO" || upper == "CURRVFO") { vfo = m_vfo; return true; }
        if (upper == "TX") {
            const int tx = m_model->txSliceArbiter()->txBoundSliceId();
            if (tx == m_binding.primarySliceId) { vfo = CatVfo::Primary; return true; }
            if (m_binding.secondarySliceId && tx == *m_binding.secondarySliceId) { vfo = CatVfo::Secondary; return true; }
        }
        return false;
    };
    if (name == "chk_vfo") {
        if (!parts.isEmpty()) { return reply(kInvalid); }
        // Native endpoint supports explicit VFO prefixes; it never follows GUI focus.
        // Hamlib 4.7.0 netrigctl.c:248-278 checks this before mandatory dump_state.
        value("ChkVFO","1"); return reply(0);
    }
    if (name == "dump_state") {
        if (!parts.isEmpty()) { return reply(kInvalid); }
        // Nereus-original serializer, schema facts from Hamlib 4.7.0
        // tests/rigctl_parse.c:4673-4890 and rigs/dummy/netrigctl.c:272-793:
        // https://raw.githubusercontent.com/Hamlib/Hamlib/4.7.0/tests/rigctl_parse.c
        // https://raw.githubusercontent.com/Hamlib/Hamlib/4.7.0/rigs/dummy/netrigctl.c
        // Version1, NET proxy model2, deprecated region0, empty RX/TX hardware
        // ranges and step lists. Terminator zeros describe absent advertised
        // hardware facts, never a getter result or guessed device capability.
        QString state = "1\n2\n0\n0 0 0 0 0 0 0\n0 0 0 0 0 0 0\n0 0\n";
        // Mode bit positions from Hamlib 4.7.0 rig.h:1382-1401; these name
        // delivered software demodulators only, with native default widths.
        const QMap<QString,int> modeBits{{"AM",0},{"CW",1},{"USB",2},{"LSB",3},{"FM",5},
            {"CWR",7},{"AMS",9},{"PKTLSB",10},{"PKTUSB",11},{"DSB",19}};
        for (auto it = kModes.cbegin(); it != kModes.cend(); ++it) {
            const auto filter = SliceModel::defaultFilterForMode(it.value());
            state += QStringLiteral("0x%1 %2\n").arg(quint64(1) << modeBits.value(it.key()),0,16).arg(filter.second-filter.first);
        }
        state += "0 0\n";
        // Actual native offset representation (SliceModel int), no IF shift,
        // unsolicited rigctld announcements, preamp or attenuator level support.
        state += QString::number(std::numeric_limits<int>::max())+'\n';
        state += QString::number(std::numeric_limits<int>::max())+"\n0\n0\n\n\n";
        // Hamlib 4.7.0 rig.h:1073,1276-1299: AF bit3, functions ANF8/APF11/
        // LOCK16/MUTE17/RIT24/XIT31. Advertise exactly implemented functions.
        constexpr quint64 kFunctionMask = (quint64(1)<<8)|(quint64(1)<<11)|(quint64(1)<<16)
            |(quint64(1)<<17)|(quint64(1)<<24)|(quint64(1)<<31);
        constexpr quint64 kAfMask = quint64(1)<<3;
        state += QStringLiteral("0x%1\n0x%1\n0x%2\n0x%2\n0\n0\n").arg(kFunctionMask,0,16).arg(kAfMask,0,16);
        // rig.h:615-623 targetable frequency/mode/function/level/RIT-XIT =0x73,
        // ptt_type enum RIG_PTT_RIG=1. PTT uses the actual selected TX target.
        state += "vfo_ops=0\nptt_type=1\ntargetable_vfo=0x73\nhas_set_vfo=1\nhas_get_vfo=1\n"
            "has_set_freq=1\nhas_get_freq=1\nhas_set_conf=0\nhas_get_conf=0\n"
            "has_power2mW=0\nhas_mW2power=0\nhas_get_ant=0\nhas_set_ant=0\n"
            "level_gran=3=0,1,0.01;\ndone\n";
        if (!extended) { return state; }
        QString result = "dump_state:"+QString(separator);
        const QStringList records = state.trimmed().split('\n');
        for (const QString& record : records) { result += record+separator; }
        return result+rprt(0);
    }
    if (name == "get_info") {
        if (!parts.isEmpty()) { return reply(kInvalid); }
        value("Info","NereusSDR"); return reply(0);
    }
    if (name == "get_lock_mode") {
        if (!parts.isEmpty()) { return reply(kInvalid); }
        const SliceModel* target = m_adapter.resolveSlice(m_binding,m_vfo);
        if (!target || !m_adapter.mayRead(m_binding,m_vfo)) { return reply(kTargetUnavailable); }
        // Native adaptation: actual selected slice LOCK, never synthetic unlocked.
        // Hamlib 4.7.0 rigctl_parse.c:381-382,5815-5845 has no ARG_OUT flag,
        // so this getter uniquely emits a bare value followed by RPRT 0.
        // netrigctl.c:2790-2809 requires both records before mode setters.
        value("Locked",target->locked() ? "1" : "0");
        return extended ? reply(0) : reply(0)+rprt(0);
    }
    if (name == "get_vfo") {
        if (!parts.isEmpty()) { return reply(kInvalid); }
        if (!m_adapter.mayRead(m_binding,m_vfo)) { return reply(kTargetUnavailable); }
        value("VFO",m_vfo == CatVfo::Primary ? "VFOA" : "VFOB"); return reply(0);
    }
    if (name == "set_vfo") {
        CatVfo requested;
        if (parts.size() != 1) { return reply(kInvalid); }
        if (!resolveVfo(parts.first(),requested)) { return reply(kVfoInvalid); }
        if (!m_adapter.mayRead(m_binding,requested)) { return reply(kTargetUnavailable); }
        m_vfo = requested; return reply(0);
    }
    if ((name == "get_split_vfo" || name == "set_split_vfo" || name == "get_ptt" || name == "set_ptt")
        && !parts.isEmpty() && vfoName(parts.first())) {
        CatVfo rx;
        if (!resolveVfo(parts.takeFirst(),rx)) { return reply(kVfoInvalid); }
        // A valid OFF releases only this session's coordinator claim; an RX
        // prefix does not impose new-action readability on surviving TX ownership.
        const bool ownedOff=name == "set_ptt" && parts.size() == 1 && parts[0] == "0";
        if (!ownedOff && !m_adapter.mayRead(m_binding,rx)) { return reply(kTargetUnavailable); }
    }
    if (name == "get_split_vfo" || name == "set_split_vfo") {
        if (name == "get_split_vfo") {
            if (!parts.isEmpty()) { return reply(kInvalid); }
            const int id = m_model->txSliceArbiter()->txBoundSliceId();
            CatVfo tx;
            if (!resolveVfo("TX",tx) || !m_adapter.mayRead(m_binding,tx)) { return reply(kTargetUnavailable); }
            value("Split",id == m_binding.primarySliceId ? "0" : "1");
            value("TX VFO",tx == CatVfo::Primary ? "VFOA" : "VFOB"); return reply(0);
        }
        if (parts.size() != 2 || (parts[0] != "0" && parts[0] != "1")) { return reply(kInvalid); }
        CatVfo tx;
        if (!resolveVfo(parts[1],tx)) { return reply(kVfoInvalid); }
        if (parts[0] == "0") { tx = CatVfo::Primary; }
        else if (tx != CatVfo::Secondary) { return reply(kInvalid); }
        const SliceModel* target = m_adapter.resolveSlice(m_binding,tx);
        if (!target) { return reply(kTargetUnavailable); }
        const bool accepted = m_coordinator.requestTxSelection(m_sessionId,target->sliceIndex());
        return reply(accepted ? 0 : kRejected);
    }
    if (name == "get_ptt" || name == "set_ptt") {
        if (name == "get_ptt") {
            if (!parts.isEmpty()) { return reply(kInvalid); }
            value("PTT",m_model->moxController()->isMox() ? "1" : "0"); return reply(0);
        }
        if (parts.size() != 1 || (parts[0] != "0" && parts[0] != "1")) { return reply(kInvalid); }
        if (parts[0] == "0") { m_coordinator.releasePtt(m_sessionId); return reply(0); }
        CatVfo tx;
        if (!resolveVfo("TX",tx)) { return reply(kRejected); }
        SliceModel* target = m_adapter.resolveSlice(m_binding,tx);
        if (!target) { return reply(kTargetUnavailable); }
        const bool accepted = m_coordinator.requestPtt(m_sessionId,target->sliceIndex());
        return reply(accepted ? 0 : kRejected);
    }
    const bool split = name.contains("split_freq") || name.contains("split_mode");
    const QString operation = split ? QString(name).replace("split_","") : name;
    static const QStringList supported{"get_freq","set_freq","get_mode","set_mode","get_freq_mode","set_freq_mode","get_rit","set_rit","get_xit","set_xit","get_level","set_level","get_func","set_func","get_ts","set_ts"};
    if (!supported.contains(operation) || (!split && (operation == "get_freq_mode" || operation == "set_freq_mode"))) { return reply(kUnavailable); }
    CatVfo vfo = split ? CatVfo::Secondary : m_vfo;
    // Strip the VFO prefix sent in chk_vfo=1 mode (#2) and resolve the slice.
    // [original inline comment from RigctlProtocol.cpp:1746]
    if (!parts.isEmpty() && vfoName(parts.first())) {
        CatVfo prefixed;
        if (!resolveVfo(parts.takeFirst(),prefixed)) { return reply(kVfoInvalid); }
        if (!split) { vfo = prefixed; }
    }
    const QPointer<SliceModel> slice(m_adapter.resolveSlice(m_binding,vfo));
    if (!slice || !m_adapter.mayRead(m_binding,vfo)) { return reply(kTargetUnavailable); }
    const bool setter = operation.startsWith("set_");
    const auto guardedWrite = [&](const QByteArray& property,const std::function<void()>& mutate,const std::function<bool()>& changed) {
        const int refusal = writeError(vfo,property); if (refusal) { return refusal; }
        const CatWriteToken token = m_adapter.prepareWrite(m_binding,vfo,property);
        mutate();
        if (!live() || !slice) { return kTargetUnavailable; }
        if (!m_adapter.revalidateWrite(token)) { return kAccessDenied; }
        return changed() ? 0 : kRejected;
    };
    struct ModePassband { DSPMode mode; int low; int high; bool preserve; };
    const auto prepareModePassband = [&](const QStringList& arguments) -> std::optional<ModePassband> {
        if (arguments.size() != 2 || !kModes.contains(arguments[0])) { return {}; }
        bool ok; const int passband = arguments[1].toInt(&ok);
        if (!ok || passband < -1) { return {}; }
        const DSPMode mode = kModes.value(arguments[0]);
        const auto edges = SliceModel::defaultFilterForMode(mode);
        const int width = passband == 0 ? edges.second-edges.first : passband;
        int low = edges.first, high = edges.second;
        if (qint64(width)+qMax(qAbs(edges.first),qAbs(edges.second)) > std::numeric_limits<int>::max()) { return {}; }
        if (width > 0) {
            if (edges.first < 0 && edges.second > 0) { low = -width/2; high = low+width; }
            else if (edges.second <= 0) { high = edges.second; low = high-width; }
            else { low = edges.first; high = low+width; }
        }
        return ModePassband{mode,low,high,passband == -1};
    };
    std::optional<ModePassband> preparedMode;
    if (operation == "get_freq" || operation == "set_freq" || operation == "get_freq_mode" || operation == "set_freq_mode") {
        const bool combined = operation.endsWith("freq_mode");
        if (!setter) {
            if (!parts.isEmpty()) { return reply(kInvalid); }
            value(split ? "TX Frequency" : "Frequency",QString::number(slice->frequency(),'f',0));
            if (!combined) { return reply(0); }
            const QString mode = modeName(slice->dspMode()); if (mode.isEmpty()) { return reply(kUnavailable); }
            value("TX Mode",mode); value("TX Passband",QString::number(slice->filterHigh()-slice->filterLow())); return reply(0);
        }
        if (parts.size() != (combined ? 3 : 1)) { return reply(kInvalid); }
        bool ok; const double hz = parts[0].toDouble(&ok);
        if (!ok || !std::isfinite(hz) || hz < 0 || hz > kMaximumIntegralFrequency) { return reply(kInvalid); }
        if (combined) {
            preparedMode = prepareModePassband(parts.mid(1));
            if (!preparedMode) { return reply(kInvalid); }
            for (const QByteArray& property : {QByteArray("frequency"),QByteArray("dspMode"),QByteArray("filterLow"),QByteArray("filterHigh")}) {
                const int refusal = writeError(vfo,property); if (refusal) { return reply(refusal); }
            }
        }
        const int error = guardedWrite("frequency",[&] { slice->setFrequency(hz); },[&] { return slice->frequency() == hz; });
        if (error || !combined) { return reply(error); }
        if (!live() || !slice) { return {}; }
        parts.removeFirst();
    }
    if (operation == "get_mode" || operation == "set_mode" || operation == "set_freq_mode") {
        if (!setter) {
            if (!parts.isEmpty()) { return reply(kInvalid); }
            const QString mode = modeName(slice->dspMode()); if (mode.isEmpty()) { return reply(kUnavailable); }
            value(split ? "TX Mode" : "Mode",mode); value(split ? "TX Passband" : "Passband",QString::number(slice->filterHigh()-slice->filterLow())); return reply(0);
        }
        if (!preparedMode) { preparedMode = prepareModePassband(parts); }
        if (!preparedMode) { return reply(kInvalid); }
        const DSPMode mode = preparedMode->mode;
        // Preserve Hamlib -1 semantics after any native frequency/band callback.
        const int low = preparedMode->preserve ? slice->filterLow() : preparedMode->low;
        const int high = preparedMode->preserve ? slice->filterHigh() : preparedMode->high;
        for (const QByteArray& property : {QByteArray("dspMode"),QByteArray("filterLow"),QByteArray("filterHigh")}) {
            const int refusal = writeError(vfo,property); if (refusal) { return reply(refusal); }
        }
        const int error = guardedWrite("dspMode",[&] { slice->setDspMode(mode); },[&] { return slice->dspMode() == mode; });
        if (error) { return reply(error); }
        // Model mode setter applies a native default filter and emits synchronously;
        // capture survival first, then revalidate authority before the second effect.
        if (!live() || !slice) { return {}; }
        for (const QByteArray& property : {QByteArray("filterLow"),QByteArray("filterHigh")}) {
            const int refusal = writeError(vfo,property); if (refusal) { return reply(refusal); }
        }
        return reply(guardedWrite("filterLow",[&] { slice->setFilter(low,high); },[&] { return slice->filterLow() == low && slice->filterHigh() == high; }));
    }
    if (operation.endsWith("rit") || operation.endsWith("xit") || operation.endsWith("ts")) {
        const bool rit = operation.endsWith("rit"), xit = operation.endsWith("xit");
        const QByteArray property = rit ? "ritHz" : xit ? "xitHz" : "stepHz";
        const auto read = [&] { return rit ? slice->ritHz() : xit ? slice->xitHz() : slice->stepHz(); };
        if (!setter) {
            if (!parts.isEmpty()) { return reply(kInvalid); }
            value(rit ? "RIT" : xit ? "XIT" : "Tuning Step",QString::number(read())); return reply(0);
        }
        if (parts.size() != 1) { return reply(kInvalid); }
        bool ok; const int offset = parts[0].toInt(&ok); if (!ok || offset == std::numeric_limits<int>::min() || (!rit && !xit && offset <= 0)) { return reply(kInvalid); }
        // Wire J/Z offsets are independent of U RIT/XIT enablement, including zero.
        // https://hamlib.sourceforge.net/html/rigctld.1.html J/Z (wire contract).
        return reply(guardedWrite(property,[&] {
            if (rit) { slice->setRitHz(offset); } else if (xit) { slice->setXitHz(offset); } else { slice->setStepHz(offset); }
        },[&] { return read() == offset; }));
    }
    const bool level = operation.endsWith("level");
    if (parts.size() != (setter ? 2 : 1)) { return reply(kInvalid); }
    const QString token = parts[0].toUpper();
    const QMap<QString,QByteArray> functions{{"RIT","ritEnabled"},{"XIT","xitEnabled"},{"LOCK","locked"},{"MUTE","muted"},{"ANF","anfEnabled"},{"APF","apfEnabled"}};
    if (token == "?") {
        if (setter) { return reply(kInvalid); }
        value(level ? "Levels" : "Funcs",level ? "AF" : functions.keys().join(' ')); return reply(0);
    }
    if (level) {
        // From AetherSDR src/core/RigctlProtocol.cpp:1334-1337,1517-1522 [@1e0718ad].
        // Native slice AF scale is 0..100; Hamlib AF is normalized 0..1.
        if (token != "AF") { return reply(kUnavailable); }
        if (!setter) { value("AF",QLocale::c().toString(slice->afGain()/100.0,'g',6)); return reply(0); }
        bool ok; const double gain = parts[1].toDouble(&ok);
        if (!ok || !std::isfinite(gain) || gain < 0 || gain > 1) { return reply(kInvalid); }
        const int native = qRound(gain*100);
        return reply(guardedWrite("afGain",[&] { slice->setAfGain(native); },[&] { return slice->afGain() == native; }));
    }
    if (!functions.contains(token)) { return reply(kUnavailable); }
    const QByteArray property = functions.value(token);
    if (!setter) { value(token,slice->property(property.constData()).toBool() ? "1" : "0"); return reply(0); }
    if (parts[1] != "0" && parts[1] != "1") { return reply(kInvalid); }
    const bool on = parts[1] == "1";
    // From AetherSDR src/core/RigctlProtocol.cpp:1786-1831 [@1e0718ad].
    // Strip the VFO prefix sent in chk_vfo=1 mode (#2) and resolve the slice.
    // [original inline comment from RigctlProtocol.cpp:1789]
    // Supported native setters replace queued Flex commands; readback is verified.
    return reply(guardedWrite(property,[&] {
        if (token == "RIT") { slice->setRitEnabled(on); }
        else if (token == "XIT") { slice->setXitEnabled(on); }
        else if (token == "LOCK") { slice->setLocked(on); }
        else if (token == "MUTE") { slice->setMuted(on); }
        else if (token == "ANF") { slice->setAnfEnabled(on); }
        else if (token == "APF") { slice->setApfEnabled(on); }
    },[&] { return slice->property(property.constData()).toBool() == on; }));
}
} // namespace NereusSDR
