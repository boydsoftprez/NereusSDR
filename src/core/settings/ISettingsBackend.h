#pragma once
// =================================================================
// src/core/settings/ISettingsBackend.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 15.
//
// The delegation seam AppSettings calls through. AppSettings gains a
// single non-owning ISettingsBackend* (AppSettings::setRemoteBackend(),
// nullptr default) and, in value()/setValue()/contains()/remove(), a
// one-branch guard clause: if a backend is installed AND it claims the
// key (handlesKey()), the call is delegated wholesale and the method
// returns immediately; otherwise execution falls through to the SAME
// body AppSettings has always run. allKeys() is additive rather than a
// guard clause: it unions its own local keys with the backend's.
// setRemoteBackend(nullptr) -- the default, and the state of every
// AppSettings instance nothing has opted into remote mode -- means every
// one of these branches is skipped on every call, which is what makes
// "byte-identical to today" true by construction rather than by
// discipline.
//
// The ONLY implementation today is SettingsProxy (SettingsProxy.h), a
// client-side cache installed on a remote-mode GUI's AppSettings
// singleton. The daemon's OWN AppSettings never has a backend installed
// -- it is the thing being proxied TO, not a proxy itself. See
// SettingsProxyServer.h for the daemon-side half, which does NOT
// implement this interface (it wraps AppSettings from the outside,
// through the existing public API plus the Task 13 change hook, rather
// than being installed as a backend of it).
//
// ---- Why a per-key handlesKey() rather than an unconditional delegate
// ---- whenever a backend is installed
//
// classifySettingsKey() (SettingsScope.h, Task 14) is the pure function
// that decides Station vs OperatorLocal. If AppSettings itself called it
// to decide whether to delegate, AppSettings.cpp would need to depend on
// src/core/settings/SettingsScope.h for a decision that is entirely the
// backend's business -- and a fake backend in a test would have no way
// to exercise "an OperatorLocal key is never even offered to me" without
// AppSettings itself hardcoding the classification rule. Putting
// handlesKey() on the interface instead means: AppSettings stays a
// generic, classification-ignorant funnel (as it always was); the real
// SettingsProxy answers handlesKey() with
// `classifySettingsKey(key) == SettingsScope::Station` (plus one named
// exception -- see SettingsProxy.h's class comment for
// kDaemonProfileSeededKey); and a test backend can answer it with
// whatever narrower rule the test actually needs to exercise (the R2
// Task 15 brief's own routing test installs one that only claims
// "hardware/").
//
// ---- The synchronous-read invariant (header invariant, per the R2
// ---- Task 15 brief) ----
//
// AppSettings::value() runs inside widget and model constructors --
// SetupDialog alone default-constructs on the order of 187 controls that
// each read a setting in their own constructor body, all on the GUI
// thread, before any event loop is spinning to service anything else.
// value(), contains() and handlesKey() on this interface MUST be:
//   - synchronous: return before the call returns, never after a later
//     event-loop turn;
//   - cache-only: answer out of memory the implementation already holds,
//     never issue a new network request, block on a socket read, or wait
//     on a condition variable to be satisfied by one;
//   - non-blocking in the OS sense: no sleep, no semaphore wait, no
//     QEventLoop::exec() / QCoreApplication::processEvents() nested
//     inside the call.
// A read that violates this works perfectly well in a unit test against
// a fake backend or a loopback connection and then deadlocks the GUI the
// first time it runs against a real link with real latency -- which is
// exactly the shape of defect that reaches a bench instead of a review.
// SettingsProxy.h's own class comment states how it satisfies this
// (everything it answers comes out of an in-memory QMap already
// populated by a PRIOR, separate applySnapshot()/setValue() call; there
// is no code path in value()/contains()/handlesKey() that can reach the
// network). tests/tst_settings_proxy.cpp's valueNeverBlocksOrSpinsEventLoop
// case is the runtime proxy for this invariant: it cannot prove the
// negative "no blocking call exists" directly, so it instead measures
// that a large batch of value() calls completes in a time far below any
// plausible network round trip, which a synchronous cache lookup does
// trivially and a hidden blocking call could not.
//
// setValue() and remove() are NOT bound by the same "never touch the
// network" rule -- a write is allowed (expected, even) to have a
// network-facing SIDE EFFECT, such as SettingsProxy::setValue() emitting
// a signal a live session relays outbound. What must stay synchronous is
// only that the CALL ITSELF returns immediately (the cache is updated
// optimistically before the call returns; the network relay, if any, is
// fire-and-forget from this interface's point of view, never awaited).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-06  J.J. Boyd / KG4VCF  Remote daemon R2 Task 15: the
//                                    AppSettings delegation interface.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Fix round 2 (review): documented the
//                                    handledKeys()/handlesKey() subset
//                                    contract Important 1's restored
//                                    allKeys() invariant now depends on.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QString>
#include <QStringList>
#include <QVariant>

namespace NereusSDR {

class ISettingsBackend {
public:
    virtual ~ISettingsBackend() = default;

    /// True if this backend is the authority for `key` and AppSettings
    /// should delegate to it instead of touching its own local
    /// m_settings map. Called before EVERY delegated operation (value,
    /// setValue, contains, remove) -- see the file header for why the
    /// per-key decision lives here rather than inside AppSettings
    /// itself. MUST be synchronous and side-effect-free: it runs inside
    /// AppSettings::value(), which runs inside widget constructors (see
    /// the file header's synchronous-read invariant).
    virtual bool handlesKey(const QString& key) const = 0;

    /// Mirrors AppSettings::value()'s own contract exactly: returns
    /// `defaultValue` for a key this backend has no value for, NEVER an
    /// empty QVariant standing in for "unset" (AppSettings.h's own
    /// value() doc comment, and GeneralOptionsPage.cpp's Region control,
    /// depend on this). Synchronous and cache-only -- see the file
    /// header's invariant paragraph.
    virtual QVariant value(const QString& key, const QVariant& defaultValue) const = 0;

    /// Mirrors AppSettings::setValue(). Permitted to have a fire-and-
    /// forget side effect (e.g. queuing an outbound wire write); must
    /// still return immediately -- see the file header.
    virtual void setValue(const QString& key, const QVariant& val) = 0;

    /// Mirrors AppSettings::contains(). Synchronous and cache-only.
    virtual bool contains(const QString& key) const = 0;

    /// Mirrors AppSettings::remove().
    virtual void remove(const QString& key) = 0;

    /// Every key this backend currently holds a REAL value for --
    /// mirrors AppSettings::allKeys()'s own contract (`m_settings.keys()`,
    /// i.e. keys with a value, not merely keys this backend COULD answer
    /// a handlesKey() query about). AppSettings::allKeys() unions this
    /// with its own local m_settings.keys(). A key this backend has
    /// proven absent on the far end (SettingsProxy's "proven-unset set",
    /// see SettingsProxy.h) is deliberately NOT included here: it has no
    /// value to enumerate, exactly as an AppSettings key that was never
    /// written is absent from m_settings.keys() today.
    ///
    /// CONTRACT (fix round 2, review, smaller item): every key returned
    /// here MUST also satisfy `handlesKey(key) == true` on this same
    /// instance. AppSettings::allKeys() (fix round 1, Important 1) relies
    /// on this: it builds its local half by excluding any m_settings key
    /// the backend CLAIMS (handlesKey()), then appends this list
    /// unmodified as the remote half. A backend that returned a key here
    /// without also claiming it via handlesKey() would silently
    /// reintroduce the exact allKeys()/contains() divergence Important 1
    /// fixed -- the key would appear in allKeys() (from this list) while
    /// contains()/value() (which gate on handlesKey() first) fall through
    /// to the LOCAL store instead of asking this backend, an inconsistent
    /// answer from two different sources. SettingsProxy satisfies this
    /// (handledKeys() returns m_cache.keys(), and every key ever inserted
    /// into m_cache arrived through a handlesKey()-gated path -- setValue()/
    /// remove() from AppSettings, or applySnapshot()/applyRemoteValue()/
    /// applyRejection(), all of which Task 18 is expected to only call
    /// with Station-scoped keys in the first place). Not mechanically
    /// enforced here (a pure virtual interface cannot assert a caller's
    /// invariant); a future backend implementation must maintain it by
    /// construction.
    virtual QStringList handledKeys() const = 0;
};

} // namespace NereusSDR
