#pragma once
// =================================================================
// src/core/settings/SettingsProxyServer.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 15.
//
// The daemon-side half of the settings mirror. Unlike SettingsProxy
// (SettingsProxyServer's client-side counterpart), this class does NOT
// implement ISettingsBackend and is never installed via
// AppSettings::setRemoteBackend() -- the daemon's own AppSettings IS the
// station's real settings store, reading and writing its own local file
// exactly as it always has. This class WRAPS that AppSettings instance
// from the outside, through its existing public API plus the Task 13
// change hook, to do two jobs: hand a connecting client a bulk snapshot
// of everything Station-scoped, and apply writes a connected client
// sends back onto the real store.
//
// ---- The hazard this class exists to not build ----
//
// Task 13's own review named this task's most likely failure by shape,
// before it was written: an inbound remote write lands via
// AppSettings::setValue(), which fires the Task 13 change hook, which
// ships the SAME change straight back out to every connected client --
// including, redundantly, the one that just sent it -- as though it were
// a brand-new local change. AppSettings.h:187-198's contract is explicit
// that the fix is NOT a suppression flag inside AppSettings itself (that
// would silently swallow a hook body's own legitimate derived writes,
// trading a loud stack overflow for a quiet data-loss bug): the CONSUMER
// of the hook must suppress its own forwarding around an inbound apply.
// This class is that consumer, and applyInboundWrite() is where the
// suppression lives -- see its doc comment and m_applyingInboundWrite
// below.
//
// The two-path design that makes this work:
//   - applyInboundWrite(key, value, originTag) is the ONLY entry point
//     for a write that originated on a connected client (Task 18 calls
//     it once per decoded inbound write). It sets m_applyingInboundWrite,
//     calls AppSettings::setValue() (which still fires the Task 13
//     hook), clears the guard, and THEN emits outboundValueChanged()
//     itself, explicitly, WITH the real originTag it was given.
//   - onLocalAppSettingsChange(key) is installed once, in the
//     constructor, as the Task 13 change hook. It is the GENERIC path:
//     it fires for EVERY AppSettings mutation on this instance,
//     including the ones applyInboundWrite() itself just caused. Its
//     FIRST action is to check m_applyingInboundWrite and return
//     immediately if true -- suppressing exactly, and only, the
//     redundant SECOND (untagged) broadcast that path would otherwise
//     produce for the identical change applyInboundWrite() is already
//     broadcasting explicitly, correctly tagged, itself.
// A change that happens for any OTHER reason (a Setup page open directly
// on the daemon's own console, if this build ever grows one; a migration
// running at startup; nereusd.conf-derived seeding) reaches
// onLocalAppSettingsChange() with m_applyingInboundWrite false, and IS
// forwarded, with an EMPTY origin tag -- there is no client to attribute
// it to, and an empty tag can never equal a real session's
// SettingsProxy::localOriginTag(), so no client will ever mistake a
// genuine daemon-local change for its own echo.
//
// tests/tst_settings_proxy.cpp's serverSuppressesEchoOnInboundApply is
// the test that proves exactly one broadcast happens per
// applyInboundWrite() call, and this task's sabotage-and-revert pass
// (see the task report) temporarily removed the m_applyingInboundWrite
// guard specifically to confirm that test fails with spy.count() == 2,
// not some unrelated assertion, before restoring it.
//
// ---- Snapshot scope (Step 5) ----
//
// buildSnapshot(connectedMac) is NOT a blanket classifySettingsKey scan
// with no MAC awareness -- hardware/ is 92% of a real settings file (the
// R2 design addendum section 8) and is inherently PER-MAC, so a scan
// with no MAC filter would hand a client every OTHER saved radio's
// hardware state too. The method instead:
//   1. Uses AppSettings::snapshot() for exactly two prefixes:
//      "hardware/<connectedMac>/" and the literal "hardware/oc/" segment
//      (Step 5's own call-out: "oc" is a fixed literal some hardware/*
//      call sites use in the MAC position -- OcOutputsHfTab.cpp's
//      pennyExtCtrl is Task 14's own canonical proof -- not a MAC, and
//      must be included explicitly because it will never equal
//      `connectedMac`).
//   2. Scans AppSettings::allKeys() once for every OTHER key (skipping
//      anything starting with "hardware/", already fully handled by
//      step 1) and includes it iff classifySettingsKey() says Station.
//      This is the "use classifySettingsKey rather than re-deriving the
//      rule" requirement: Task 14's own prefix table is a private,
//      unexported implementation detail of SettingsScope.cpp, so the
//      only way to stay in sync with it without hand-copying it here is
//      to ask the function per key rather than guess its family list.
//   3. Includes AppSettings::kDaemonProfileSeededKey explicitly,
//      unconditionally, whenever AppSettings::instance().contains() says
//      it is present -- see SettingsProxy.h's own "Setup-dialog gate"
//      paragraph for why: it is not a Station "setting" in
//      classifySettingsKey's sense at all (no rule there names it), it
//      is this protocol's own connect-time bookkeeping, so step 2 would
//      never pick it up on its own.
//
// ---- Inbound-write validation is scope-only, with named exceptions ----
// ---- (fix round 1 review, Important 3; whole-branch review, Minor 7) ----
//
// applyInboundWrite() checks classifySettingsKey() == Station and
// nothing else about a write's VALUE, by design -- see its own doc
// comment for why a general value-validation framework does not belong
// here. A fix-round review found that the natural-sounding fallback,
// SettingsHygiene (R2 design addendum section 6.4's "the daemon's
// station-value validator"), is not actually wired into any write path
// at all: it runs exactly once, after a successful connect
// (RadioModel.cpp), never on a mutation; it only builds an advisory
// QVector<Issue> rather than clamping or rejecting anything; and its two
// subscribers are GUI diagnostics pages nereusd does not link. An
// out-of-range Station value written over the wire today reaches
// AppSettings with nothing between the socket and the applied state.
//
// One key gets a targeted bounds check anyway: SwrProtectionLimit
// (whole-branch review, Minor 7): Station-scoped, reaching
// SwrProtectionController::setLimit(), which stores without clamping,
// while the only UI that writes it is a QDoubleSpinBox pinned to
// 1.0..5.0. Bounded to that same 1.0..5.0 in .cpp, so the wire cannot
// express a limit the operator's own control cannot -- an authenticated
// client could otherwise park it at 99 (protection effectively off after
// the next daemon start) or below 1.0 (unreachable, so the gate trips
// permanently).
//
// The step attenuator keys (options/stepAtt/rx1Value, rx1Band/<band>,
// txBand/<band>) used to get a unioned bounds check here too (fix rounds
// 1-3). R-R3-46 / R-R3-11 made every options/stepAtt, options/autoAtt and
// options/preamp key the Core's own (isModelOwnedDspSettingsKey), refused
// before any value is read whatever it is, so that check could no longer
// be reached and was removed (R3 remote radio hardware plan, Task 3). A
// window changes those values through the `stepAtt` object, where the
// Core's controller applies its own radio's range.
//
// Every other un-gated numeric Station value (per-band preamp, sample
// rate catalogue entries not already checked by resolveSampleRate, etc.)
// is explicitly OUT of this fix's scope; the general gap is tracked as a
// hostile-value row in Task 20's acceptance run, not solved piecemeal
// here.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-06  J.J. Boyd / KG4VCF  Remote daemon R2 Task 15: daemon-side
//                                    settings snapshot + inbound-apply
//                                    server. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Fix round 1 (review): Important 3
//                                    (step-attenuator bounds check,
//                                    corrected SettingsHygiene claim),
//                                    Minor 5 (exception-safe
//                                    m_applyingInboundWrite via
//                                    QScopeGuard), Minor 6 (both
//                                    broadcast paths now emit
//                                    m_appSettings.value(key) so the
//                                    QVariant type is consistent). AI-
//                                    assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Fix round 2 (review): Important 3
//                                    corrected in both directions --
//                                    the union range now includes
//                                    stepAttMaxDb()'s Alex-widened
//                                    ceiling (was silently rejecting
//                                    legitimate 32-61 dB settings on
//                                    five board types), and the gate now
//                                    also covers rx1Band/<band> and
//                                    txBand/<band>, the sibling keys
//                                    that were bypassing the original
//                                    rx1Value-only check entirely. AI-
//                                    assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Fix round 3 (review): Important 3 TX
//                                    half -- fix round 2's hardcoded 0 dB
//                                    TX floor rejected legitimate
//                                    negative HL2 TX values that
//                                    setAttOnTxValue() (the PureSignal
//                                    AutoAtt write path) deliberately
//                                    lets through. All three key families
//                                    now share one union floor. AI-
//                                    assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 4:
//                                    outboundValueRemoved(), so a removal
//                                    is not reported as a value change.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Minor 7:
//                                    SwrProtectionLimit is range-checked
//                                    on the wire path against its own
//                                    spinbox's 1.0..5.0. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: the step
//                                    attenuator range check removed; its
//                                    keys are refused earlier as the
//                                    Core's own. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 72 (R-IOS-02): the
//                                    signals below reach every device's
//                                    session; a refusal goes to the writer
//                                    alone (StationServer). AI-assisted
//                                    via Anthropic Claude Code.
// =================================================================

#include <QMap>
#include <QObject>

#include <functional>
#include <QString>
#include <QVariant>

namespace NereusSDR {

class AppSettings;

/// Outcome of one applyInboundWrite() call. Shape matches
/// StateMirror.h's MirrorApplyResult (accepted / reason) for the same
/// class of "was this write allowed" answer, plus a settings-specific
/// restoredValue for the rejection case.
struct SettingsApplyResult {
    /// True iff the write landed. False leaves the daemon's real
    /// AppSettings store exactly as it was.
    bool accepted = false;

    /// Empty iff accepted.
    QString reason;

    /// Meaningful only when !accepted: the daemon's OWN current value for
    /// `key` (invalid QVariant if the daemon has nothing for it either --
    /// proven-unset, not an empty string), for the caller to relay back
    /// to the offending client as SettingsProxy::applyRejection()'s
    /// `restoredValue`.
    QVariant restoredValue;
};

class SettingsProxyServer : public QObject {
    Q_OBJECT

public:
    /// `appSettings` is the daemon's OWN settings store -- normally
    /// AppSettings::instance() on a real nereusd, or an isolated
    /// AppSettings(tempPath) in a test. Non-owning; caller keeps it
    /// alive for at least this object's lifetime. Installs this
    /// instance's change hook on `appSettings` (see the class comment);
    /// the destructor clears it again so a SettingsProxyServer that
    /// outlives its own usefulness never leaves a dangling `this`
    /// captured in a std::function on an AppSettings instance that
    /// outlives it (AppSettings::instance() is a function-local static
    /// with effectively unbounded lifetime).
    ///
    /// PRECONDITION, matching StateMirror's own (StateMirror.h): this
    /// object, `appSettings`, and whatever calls applyInboundWrite() must
    /// all run on the SAME thread. AppSettings has no internal locking
    /// (AppSettings.h:200-203) and neither does this class.
    explicit SettingsProxyServer(AppSettings& appSettings, QObject* parent = nullptr);
    ~SettingsProxyServer() override;

    SettingsProxyServer(const SettingsProxyServer&) = delete;
    SettingsProxyServer& operator=(const SettingsProxyServer&) = delete;

    /// See the class comment's "Snapshot scope" section. `connectedMac`
    /// may be empty (no per-MAC hardware/ subtree is then included at
    /// all -- every other Station-scoped key, plus the seed marker, still
    /// is); a real caller always has a connected MAC by the time it asks
    /// for a snapshot, but an empty-MAC test fixture should not crash.
    QMap<QString, QString> buildSnapshot(const QString& connectedMac) const;

    /// Applies one inbound write a connected client sent for `key`. See
    /// the class comment for the anti-echo suppression this performs
    /// around the underlying AppSettings::setValue() call.
    ///
    /// Rejects (accepted=false, nothing changes) when `key` does not
    /// classify Station -- a well-behaved client's own SettingsProxy
    /// never offers a non-Station key over the wire in the first place
    /// (ISettingsBackend::handlesKey()), so reaching this path at all
    /// means a stale, buggy, or hostile client sent something outside
    /// the protocol; this is defense in depth, not the primary gate.
    ///
    /// General value-bounds / range validation for a key that IS
    /// Station-scoped is explicitly NOT this method's job -- see the
    /// class comment's "Inbound-write validation" section for why
    /// SettingsHygiene is NOT that fallback despite the name suggesting
    /// it (a fix-round review found it wired into no write path at all),
    /// and for the four specific, targeted exceptions this method DOES
    /// enforce (the step-attenuator rx1Value / rx1Band / txBand union
    /// bounds check, and SwrProtectionLimit's flat 1.0..5.0 range, .cpp).
    SettingsApplyResult applyInboundWrite(const QString& key, const QVariant& value,
                                          const QString& originTag);

    /// R-R3-46: the MAC of the radio the Core is connected to now, asked
    /// at every write. With a provider set, a hardware/<mac>/ write (or
    /// remove, otherRadioRefusal()) for any other MAC, or for any MAC while
    /// no radio is connected, is refused: the Core applies hardware
    /// settings for its own radio only. hardware/oc/ is a literal segment,
    /// not a MAC, and is never refused here. Without a provider (older
    /// callers and fixtures) nothing is checked.
    void setConnectedMacProvider(std::function<QString()> provider)
    {
        m_connectedMac = std::move(provider);
    }

    /// The plain reason a write or remove of `key` is refused because it
    /// names another radio, or an empty string when it does not.
    QString otherRadioRefusal(const QString& key) const;

    /// iPhone app Task 75 (the several-devices design, ruling 7.1): the
    /// settings keys whose write reaches other devices' slices or the
    /// transmitter, by what they touch. A write of one of these goes
    /// through the Core's confirm step (StationServer) before it is
    /// applied; every other key applies at once, as before.
    enum class SharedFamily {
        None,
        /// DspOptions<Setting><Group>Rx: every receiver.
        ReceiveOptions,
        /// PGXL_...: the amplifier, its interlock and power limit (the
        /// transmitter).
        Amplifier,
        /// TGXL_... and RfKit_...: the tuner and the RF-Kit amplifier's
        /// antenna (ADC0's receivers, or every receiver on a 1-ADC board,
        /// and the transmitter).
        Tuner,
        /// The transmitter's own Core settings (merge of the trunk into the
        /// transmit lane, with Task 34's holder): External TX Inhibit
        /// (TxInhibitMonitorEnabled, TxInhibitMonitorReversed) and Receive
        /// Only (RxOnly); and (trunk merge of remote transmit) the Alex
        /// tab's three transmit high-pass switches
        /// (isAlexHpfTransmitSwitchKey). The transmit DSP options and the transmit
        /// object's settings are a permitted session's (StationServer's
        /// transmit gate), so a device that does not hold transmit is
        /// refused them rather than asked.
        Transmitter,
    };
    static SharedFamily sharedFamilyOf(const QString& key);
    /// R-R3-46 (parity Task 14): hardware/.../alex/master/{hpfBypassOnTx,
    /// hpfBypassOnPs,disable6mLnaOnTx}, the Alex tab's three transmit
    /// high-pass switches (case-blind).
    static bool isAlexHpfTransmitSwitchKey(const QString& key);

signals:
    /// One Station-key change worth telling every connected client
    /// about: either a genuine local/daemon-side change (originTag
    /// empty), or the deliberate, explicitly-tagged echo of an
    /// applyInboundWrite() call (originTag is exactly what that call was
    /// given). Never fired twice for the same underlying setValue() call
    /// -- see the class comment.
    ///
    /// iPhone app Task 72 (ruling 5.8): StationServer sends it as
    /// settings.value to every device's session, each of which holds every
    /// Station key, keeping the writer's originTag so the writer alone
    /// recognises its own echo. A refused write (applyInboundWrite's
    /// SettingsApplyResult) becomes settings.reject to the writer only.
    void outboundValueChanged(const QString& key, const QVariant& value, const QString& originTag);

    /// One Station key that is now GONE from the station's store, rather
    /// than holding a new value. A separate signal, not an
    /// outboundValueChanged carrying an invalid QVariant.
    ///
    /// Whole-branch review, Important 4. The generic hook path cannot
    /// tell a caller which kind of mutation happened, and it reported
    /// both as a value change: AppSettings::value() on an absent key
    /// returns an INVALID QVariant, StationServer flattened that with
    /// .toString() into "", and the client cached an empty string for a
    /// key the station no longer had -- contains() true on one side and
    /// false on the other, with value(key, someDefault) returning ""
    /// instead of the caller's default. AppSettings::contains() is the
    /// question that actually distinguishes the two, so it is asked here
    /// and the answer is carried in the signal's identity rather than in
    /// the validity of a QVariant that has to survive a relay.
    ///
    /// Origin tag deliberately absent: every removal reaching this class
    /// arrives through the generic local-change hook (there is no
    /// applyInboundRemove counterpart to applyInboundWrite -- StationServer
    /// routes a client's remove straight at the store), so there is never
    /// a tag to carry, exactly as onLocalAppSettingsChange's value path
    /// already emits an empty one.
    void outboundValueRemoved(const QString& key);

private:
    /// Installed as m_appSettings's Task 13 change hook. See the class
    /// comment for the full anti-echo mechanism.
    void onLocalAppSettingsChange(const QString& key);

    AppSettings& m_appSettings;

    std::function<QString()> m_connectedMac;

    /// True for the duration of one applyInboundWrite() call. Checked
    /// first in onLocalAppSettingsChange(), before that method does
    /// anything else, so it suppresses the generic broadcast path for
    /// exactly the write currently in flight through applyInboundWrite()
    /// -- see the class comment's two-path explanation. Re-entrancy is
    /// not the concern a save/restore guard would need to solve here
    /// (unlike StateMirror's m_applying, StateMirror.h, which DOES need
    /// save/restore because a command handler can nest a second
    /// applyInbound() inside the first): applyInboundWrite() makes
    /// exactly one AppSettings::setValue() call and that call cannot
    /// synchronously re-invoke applyInboundWrite().
    ///
    /// Fix round 1 (review, Minor 5): the field is still set/cleared via
    /// a QScopeGuard in the .cpp, not a bare `= true; ...; = false;`
    /// pair -- the risk there was never re-entrancy, it was exception
    /// safety. If AppSettings::setValue() (or anything the Task 13
    /// change hook chain calls) ever threw, a bare pair would leave this
    /// flag stuck at true for the object's ENTIRE remaining lifetime,
    /// silently suppressing every subsequent genuine daemon-local
    /// Station change's broadcast forever -- permanent silent data loss
    /// on connected clients' views, not a crash, and far harder to
    /// notice than one.
    bool m_applyingInboundWrite = false;
};

} // namespace NereusSDR
