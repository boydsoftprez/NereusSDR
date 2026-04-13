// tests/TestSandboxInit.cpp
//
// Auto-linked into every test binary by nereus_add_test(). Runs
// QStandardPaths::setTestModeEnabled(true) as a global static
// constructor — before main(), before any test code, and crucially
// before AppSettings::instance() can pin its file path to the real
// ~/.config/NereusSDR/NereusSDR.settings.
//
// Why this exists: AppSettings is a process-global singleton whose
// default constructor calls QStandardPaths::writableLocation(...) to
// resolve the settings file. Once resolved, the path is cached for
// the life of the process. If any test touches AppSettings::instance()
// without first enabling test mode, the singleton pins to the real
// user config directory and subsequent setValue/save calls overwrite
// the developer's actual NereusSDR.settings — wiping saved radios,
// splitter sizes, VFO state, display prefs, etc. This was observed
// during the v0.1.1 alpha when a ctest run silently replaced a
// populated 68-key settings file with a 252-key file full of
// hardware entries keyed to the P1FakeRadio test MAC
// "aa:bb:cc:11:22:33". See fix(radio) commit for the session that
// tracked it down.
//
// QStandardPaths::setTestModeEnabled(true) is safe to call multiple
// times and has no side effects other than redirecting the writable-
// location lookups to a per-user, per-app sandbox path under
// $XDG_CONFIG_HOME/qttest (Linux), ~/Library/Preferences/qttest
// (macOS), or %LOCALAPPDATA%\qttest (Windows). The sandbox is shared
// across all test binaries in a run but is completely isolated from
// the real NereusSDR install.
//
// This file has no public symbols. It is linked into every test
// target by nereus_add_test() in tests/CMakeLists.txt.

#include <QStandardPaths>

namespace {

struct SandboxInit {
    SandboxInit()
    {
        QStandardPaths::setTestModeEnabled(true);
    }
};

// Global static constructor runs before main(). The only dependency
// is QStandardPaths, a stateless Qt utility that does not require
// QCoreApplication to be constructed.
static SandboxInit g_nereusTestSandboxInit;

} // namespace
