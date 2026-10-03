// no-port-check: NereusSDR-original test support; no Thetis logic.
// =================================================================
// tests/TestFftwWisdomCache.cpp  (NereusSDR)
// =================================================================
//
// Linked by nereus_add_test() into every test that starts WDSP in its own
// process without WDSP's wisdom step: WdspEngine::setSynchronousInitForTest,
// DaemonApp's synchronous WDSP for tests, the ConnectableRadioModel harness,
// and the WdspEngine friend seam that primes m_initialized and opens real
// channels. None of those loads a wisdom file, so every FFTW plan the WDSP
// channels make (FFTW_PATIENT) is planned from nothing: 45 to 85 s of
// processor time per test on a Mac, which is what pushed five of them past
// ctest's 120 s limit under load.
//
// The plans FFTW made are kept in one file per build directory, beside the
// test binaries (NEREUS_TEST_FFTW_WISDOM_FILE), and handed back to FFTW
// before main() runs, so the next test to open the same channels finds
// them. At exit (after main()
// returns, so the test object and every model and WDSP channel it held are
// gone and no thread is planning) the process merges the file's current
// plans with its own under a lock file and replaces the file whole, and
// only when it learnt something: tests that run side by side never read
// half a file, and a test that finishes later keeps the plans an earlier
// one wrote. A missing or unreadable file, or a lock not got within two
// seconds, costs the old planning time and nothing more.
//
// The exit step uses the C++ library only, not Qt: it runs from atexit,
// also in tests without a QCoreApplication (QTEST_APPLESS_MAIN), when Qt's
// own globals may already be gone.
//
// Group B fix wave (I3) did this for the ConnectableRadioModel harness
// alone (connectable-radio-fftw-wisdom); the parity mini-round moved it
// here, for every such test, and removed the harness's own copy.
//
// R-R3-49 load round: the cache alone left every test's time hanging on
// the file's state. A plan the file does not hold yet (a new build
// directory, as on CI; a new channel size; a test killed at its limit,
// which saves nothing) was planned from nothing again, so the WDSP tests of
// a first suite run all planned at once and pushed one another past the
// limit: tst_wdsp_thread_hook took 96.8 s cold (37 s of processor time) at
// load 117 and 0.48 s warm. Each plan search in a test process is now
// capped at kPlanTimeLimitSeconds (fftw_set_timelimit), so a cold run costs
// seconds: 1.9 s for that test at load 86. Plans the file holds are reused
// as they are. What a plan computes does not change, only how long FFTW
// searches for the fastest way to compute it; FFTW_PATIENT already picks
// by timing, so no test could rely on which plan it got. The Core itself
// keeps its full search and its own wisdom file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 parity mini-round: created
//                                    from the harness's cache.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  R-R3-49 load round: each plan search
//                                    capped (kPlanTimeLimitSeconds).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

#include <fftw3.h>

namespace {

namespace fs = std::filesystem;

constexpr const char* kCachePath = NEREUS_TEST_FFTW_WISDOM_FILE;
constexpr auto kLockWait = std::chrono::seconds(2);
constexpr auto kStaleLock = std::chrono::seconds(30);
// The longest one plan search may take in a test process (see above).
// 0.1 s: tst_wdsp_thread_hook's two channels plan in 1.9 s cold at load 86
// (0.02 s: 0.9 s; 0.5 s: 6.3 s; no cap: 96.8 s at load 117).
constexpr double kPlanTimeLimitSeconds = 0.1;

std::string readCache()
{
    std::ifstream in(kCachePath, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string exportedWisdom()
{
    char* wisdom = fftw_export_wisdom_to_string();
    if (wisdom == nullptr) {
        return {};
    }
    std::string out(wisdom);
    fftw_free(wisdom);
    return out;
}

// What this process started from; written back only if it grew.
std::string& importedAtStart()
{
    static std::string imported;
    return imported;
}

// An exclusive lock file ("wx" creates it only if it is not there), taken
// for the read-merge-replace step. A lock older than kStaleLock belongs to
// a test that died holding it.
class LockFile {
public:
    LockFile()
        : m_path(std::string(kCachePath) + ".lock")
    {
        const auto deadline = std::chrono::steady_clock::now() + kLockWait;
        while (true) {
            if (std::FILE* f = std::fopen(m_path.c_str(), "wx")) {
                std::fclose(f);
                m_held = true;
                return;
            }
            std::error_code ec;
            const auto written = fs::last_write_time(m_path, ec);
            if (!ec && fs::file_time_type::clock::now() - written > kStaleLock) {
                fs::remove(m_path, ec);
                continue;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    ~LockFile()
    {
        if (m_held) {
            std::error_code ec;
            fs::remove(m_path, ec);
        }
    }
    LockFile(const LockFile&) = delete;
    LockFile& operator=(const LockFile&) = delete;
    bool held() const { return m_held; }

private:
    std::string m_path;
    bool m_held{false};
};

void saveMergedWisdom()
{
    const std::string own = exportedWisdom();
    if (own.empty() || own == importedAtStart()) {
        return;
    }
    LockFile lock;
    if (!lock.held()) {
        return;
    }
    // Plans another test wrote since this one started join ours first.
    const std::string current = readCache();
    if (!current.empty()) {
        fftw_import_wisdom_from_string(current.c_str());
    }
    const std::string merged = exportedWisdom();
    if (merged.empty() || merged == current) {
        return;
    }
    // Written beside the file and then put in its place, so a reader sees
    // the old file or the new one, never half of one.
    std::ostringstream name;
    name << kCachePath << ".tmp." << std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string temp = name.str();
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return;
        }
        out << merged;
        if (!out.flush()) {
            std::error_code ec;
            fs::remove(temp, ec);
            return;
        }
    }
    std::error_code ec;
    fs::rename(temp, kCachePath, ec);
    if (ec) {
        fs::remove(temp, ec);
    }
}

struct FftwWisdomCache {
    FftwWisdomCache()
    {
        // Before main(), so before any model exists and any thread plans.
        fftw_set_timelimit(kPlanTimeLimitSeconds);
        const std::string wisdom = readCache();
        if (!wisdom.empty() && fftw_import_wisdom_from_string(wisdom.c_str()) != 0) {
            importedAtStart() = exportedWisdom();
        }
        std::atexit(saveMergedWisdom);
    }
};

static FftwWisdomCache g_nereusTestFftwWisdomCache;

} // namespace
