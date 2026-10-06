#!/usr/bin/env python3
"""Store readiness checks for NereusSDR for iPhone and iPad (R-IOS-28).

- The privacy label says "Data Not Collected": nothing in the app, its
  widget, the shared code or NereusKit may import or link an analytics,
  advertising or tracking framework, depend on one as a package, or name a
  known tracking endpoint, and the app's Info.plist asks for no tracking.
- The Info.plist's ITSAppUsesNonExemptEncryption and
  ITSEncryptionExportComplianceCode agree with the answer and the code
  recorded in ios/AppStore/export-compliance.md.
- The About screen's Corresponding Source and Privacy links point at the
  website's pages, which exist and are in the sitemap.
- The app carries a privacy manifest (NereusApp/Resources/PrivacyInfo.xcprivacy,
  ITMS-91053): no tracking, no tracking domains, no data collected, and
  every required-reason API category the shipped code uses declared with
  Apple's approved reason. With NEREUS_IOS_RELEASE_APP set to a built
  Release NereusSDR.app, the manifest must be in it, and every category
  whose API the app's or the widget's binary calls must be declared in that
  binary's bundle.

Runs under pytest or alone:
`python3 tests/compliance/test_ios_store_readiness.py`.
"""

from __future__ import annotations

import json
import os
import plistlib
import re
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Dict, Iterable, List, Set
from urllib.parse import urlparse

REPO = Path(__file__).resolve().parents[2]
IOS = REPO / "ios"
SITE = REPO / "website" / "public"

# Where the shipped code and its build description live.
SCANNED_DIRS = ("NereusApp", "NereusActivity", "Shared", "NereusKit/Sources")
SCANNED_FILES = ("project.yml", "NereusKit/Package.swift")
SOURCE_SUFFIXES = {".swift", ".m", ".mm", ".h", ".c", ".cpp", ".yml", ".yaml",
                   ".json", ".plist", ".xcconfig", ".entitlements"}

# Module and framework names of analytics, advertising, attribution,
# crash-reporting and tracking SDKs, and Apple's own advertising and
# tracking frameworks. Matched only as an import, a linked framework or a
# package product, never as a plain word.
TRACKING_MODULES = (
    "AdSupport", "AppTrackingTransparency", "AdServices", "iAd",
    "StoreKitAdNetwork",
    "Firebase", "FirebaseCore", "FirebaseAnalytics", "FirebaseCrashlytics",
    "FirebasePerformance", "FirebaseInAppMessaging", "FirebaseMessaging",
    "GoogleAnalytics", "GoogleMobileAds", "GoogleAppMeasurement",
    "GoogleTagManager", "UserMessagingPlatform",
    "Crashlytics", "Fabric",
    "FBSDKCoreKit", "FBSDKLoginKit", "FBAudienceNetwork", "FacebookCore",
    "FacebookAEM",
    "Mixpanel", "Amplitude", "AmplitudeSwift", "Segment", "AnalyticsSwift",
    "AppsFlyerLib", "AdjustSdk", "Adjust", "BranchSDK", "Branch",
    "Flurry_iOS_SDK", "FlurryAnalytics",
    "Sentry", "SentrySwiftUI", "Bugsnag", "BugsnagPerformance", "Instabug",
    "TelemetryDeck", "TelemetryClient", "PostHog", "Heap", "HeapIOSAutocapture",
    "AppCenter", "AppCenterAnalytics", "AppCenterCrashes",
    "OneSignal", "OneSignalFramework", "KochavaTracker", "KochavaCore",
    "Singular", "UnityAds", "AppLovinSDK", "IronSource", "Countly",
    "DatadogCore", "DatadogRUM", "NewRelic", "Smartlook", "UXCam", "Pendo",
    "BrazeKit", "Appboy", "Leanplum", "CleverTapSDK", "Localytics",
    "ADEUMInstrumentation", "EmbraceIO", "Embrace", "MetricKitAnalytics",
    "Chartboost", "VungleSDK", "InMobiSDK", "MoPubSDK",
)

# Hosts that only analytics, advertising or tracking services answer on.
TRACKING_HOSTS = (
    "app-measurement.com", "google-analytics.com", "googletagmanager.com",
    "doubleclick.net", "googleadservices.com", "googlesyndication.com",
    "firebaselogging", "crashlytics.com", "graph.facebook.com",
    "api.mixpanel.com", "api2.amplitude.com", "api.amplitude.com",
    "api.segment.io", "cdn.segment.com", "appsflyer.com", "app.adjust.com",
    "api2.branch.io", "sentry.io", "bugsnag.com", "instabug.com",
    "nom.telemetrydeck.com", "posthog.com", "heapanalytics.com",
    "in.appcenter.ms", "onesignal.com", "kochava.com", "singular.net",
    "unityads.unity3d.com", "applovin.com", "flurry.com",
)

# Info.plist keys that exist only for tracking or ad attribution.
TRACKING_PLIST_KEYS = (
    "NSUserTrackingUsageDescription", "SKAdNetworkItems",
    "NSAdvertisingAttributionReportEndpoint", "GADApplicationIdentifier",
)

_NAMES = "|".join(sorted((re.escape(n) for n in TRACKING_MODULES), key=len, reverse=True))
TRACKING_USE_PATTERNS = (
    # Swift, Objective-C and C imports.
    re.compile(r"^\s*(?:@_exported\s+|@testable\s+|@preconcurrency\s+)*import\s+"
               r"(?:(?:struct|class|enum|protocol|func|var|let|typealias)\s+)?(" + _NAMES + r")\b",
               re.M),
    re.compile(r"^\s*@import\s+(" + _NAMES + r")\b", re.M),
    re.compile(r"^\s*#\s*(?:import|include)\s*[<\"](" + _NAMES + r")/", re.M),
    # SwiftPM and XcodeGen linking.
    re.compile(r"\.linkedFramework\(\s*\"(" + _NAMES + r")\"", re.M),
    re.compile(r"-framework[\"',\s]+(" + _NAMES + r")\b", re.M),
    re.compile(r"\b(?:framework|sdk):\s*\"?(" + _NAMES + r")\.framework\b", re.M),
    re.compile(r"\bproduct:\s*\"?(" + _NAMES + r")\"?\s*$", re.M),
    re.compile(r"\.product\(\s*name:\s*\"(" + _NAMES + r")\"", re.M),
)
PACKAGE_URL = re.compile(r"\.package\(\s*(?:name:\s*\"[^\"]*\",\s*)?url:", re.M)


def _files(ios: Path) -> Iterable[Path]:
    for name in SCANNED_FILES:
        path = ios / name
        if path.is_file():
            yield path
    for name in SCANNED_DIRS:
        base = ios / name
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                yield path
    for resolved in sorted(ios.rglob("Package.resolved")):
        if "DerivedData" not in resolved.parts and ".build" not in resolved.parts:
            yield resolved


def find_tracking(ios: Path) -> List[str]:
    """Every sign of an analytics, advertising or tracking framework."""
    findings: List[str] = []
    for path in _files(ios):
        shown = "ios/" + path.relative_to(ios).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        if path.name == "Package.resolved":
            findings.append(shown + ": a resolved remote package; the app depends on none")
            continue
        for pattern in TRACKING_USE_PATTERNS:
            for match in pattern.finditer(text):
                line = text.count("\n", 0, match.start()) + 1
                findings.append("%s:%d: uses %s" % (shown, line, match.group(1)))
        lowered = text.lower()
        for host in TRACKING_HOSTS:
            if host in lowered:
                findings.append("%s: names the tracking host %s" % (shown, host))
        if path.name == "Package.swift" and PACKAGE_URL.search(text):
            findings.append(shown + ": depends on a remote package")
        if path.suffix == ".plist":
            for key in TRACKING_PLIST_KEYS:
                if "<key>%s</key>" % key in text:
                    findings.append("%s: declares %s" % (shown, key))
    project = ios / "project.yml"
    if project.is_file():
        packages = re.search(r"^packages:\n((?:[ \t]+.*\n|\n)*)", project.read_text(encoding="utf-8"), re.M)
        if packages and re.search(r"^\s+url:", packages.group(1), re.M):
            findings.append("ios/project.yml: depends on a remote package")
    return findings


EXPORT_ANSWER = re.compile(
    r"^ITSAppUsesNonExemptEncryption: (YES-in-App-Store-Connect|YES|NO|pending)\s*$", re.M)
EXPORT_CODE = re.compile(r"^ITSEncryptionExportComplianceCode: (\S+)\s*$", re.M)


def export_answer(ios: Path) -> str:
    text = (ios / "AppStore" / "export-compliance.md").read_text(encoding="utf-8")
    found = EXPORT_ANSWER.findall(text)
    if len(found) != 1:
        raise AssertionError("export-compliance.md must carry exactly one "
                             "'ITSAppUsesNonExemptEncryption: "
                             "YES-in-App-Store-Connect|YES|NO|pending' line")
    return found[0]


def export_code(ios: Path) -> str:
    text = (ios / "AppStore" / "export-compliance.md").read_text(encoding="utf-8")
    found = EXPORT_CODE.findall(text)
    if len(found) != 1:
        raise AssertionError("export-compliance.md must carry exactly one "
                             "'ITSEncryptionExportComplianceCode: <code>|none' line")
    return found[0]


def export_disagreements(answer: str, code: str, plist: dict) -> List[str]:
    """What the app's Info.plist gets wrong for the recorded answer and code."""
    problems: List[str] = []
    key = "ITSAppUsesNonExemptEncryption"
    code_key = "ITSEncryptionExportComplianceCode"
    if answer in ("YES-in-App-Store-Connect", "pending"):
        if key in plist:
            problems.append(f"the record says {answer}, so {key} must stay out")
        if code_key in plist:
            problems.append(f"the record says {answer}, so {code_key} must stay out")
    elif answer == "YES":
        if plist.get(key) is not True:
            problems.append(f"the record says YES, so {key} must be true")
        if code == "none":
            problems.append("the record says YES, so its code line must hold the "
                            "code Apple issued, not none")
        elif plist.get(code_key) != code:
            problems.append(f"the record says YES, so {code_key} must equal the "
                            "record's code line")
    elif answer == "NO":
        if plist.get(key) is not False:
            problems.append(f"the record says NO, so {key} must be false")
    return problems


def site_page(url: str) -> Path:
    """The file Caddy serves for a nereussdr.com URL (try_files {path} {path}.html)."""
    parsed = urlparse(url)
    assert parsed.scheme == "https" and parsed.netloc == "nereussdr.com", url
    path = parsed.path.lstrip("/")
    direct = SITE / path
    return direct if direct.is_file() else SITE / (path + ".html")


class TrackingFrameworks(unittest.TestCase):
    def test_the_app_uses_no_tracking_framework(self) -> None:
        self.assertEqual(find_tracking(IOS), [])

    def test_the_scan_finds_each_kind_of_use(self) -> None:
        cases = {
            "NereusApp/A.swift": "import Foundation\nimport AppTrackingTransparency\n",
            "NereusApp/B.swift": "@preconcurrency import FirebaseAnalytics\n",
            "NereusApp/C.m": "#import <AdSupport/AdSupport.h>\n",
            "NereusApp/D.m": "@import GoogleMobileAds;\n",
            "NereusApp/E.swift": "let u = \"https://api.mixpanel.com/track\"\n",
            "NereusKit/Sources/X/F.swift": "import class Sentry.SentrySDK\n",
            "NereusApp/Info.plist": "<plist><dict><key>NSUserTrackingUsageDescription</key>"
                                    "<string>x</string></dict></plist>\n",
            "NereusKit/Package.swift": "let p = Package(dependencies: [.package(url: \"x\", from: \"1\")],\n"
                                       "  linkerSettings: [.linkedFramework(\"AdServices\")])\n",
            "project.yml": "packages:\n  Amp:\n    url: https://example.invalid/amp\n"
                           "targets:\n  A:\n    dependencies:\n      - sdk: AdSupport.framework\n"
                           "      - package: Amp\n        product: AmplitudeSwift\n",
        }
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for relative, text in cases.items():
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            (root / "NereusKit" / "Package.resolved").write_text("{}", encoding="utf-8")
            findings = "\n".join(find_tracking(root))
        for expected in ("A.swift:2: uses AppTrackingTransparency",
                         "B.swift:1: uses FirebaseAnalytics",
                         "C.m:1: uses AdSupport", "D.m:1: uses GoogleMobileAds",
                         "names the tracking host api.mixpanel.com",
                         "F.swift:1: uses Sentry",
                         "declares NSUserTrackingUsageDescription",
                         "uses AdServices", "Package.swift: depends on a remote package",
                         "uses AdSupport", "uses AmplitudeSwift",
                         "project.yml: depends on a remote package",
                         "Package.resolved: a resolved remote package"):
            self.assertIn(expected, findings)

    def test_plain_words_are_not_frameworks(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "NereusApp").mkdir()
            (root / "NereusApp" / "A.swift").write_text(
                "// Adjust the branch of the segment; heap and sentry are words.\n"
                "import Foundation\nlet adjust = 1\n", encoding="utf-8")
            self.assertEqual(find_tracking(root), [])


class ExportCompliance(unittest.TestCase):
    def test_plist_key_matches_the_recorded_answer(self) -> None:
        with (IOS / "NereusApp" / "Info.plist").open("rb") as handle:
            plist = plistlib.load(handle)
        self.assertEqual(export_disagreements(export_answer(IOS), export_code(IOS), plist), [])

    def test_yes_in_app_store_connect_and_pending_need_both_keys_out(self) -> None:
        for answer in ("YES-in-App-Store-Connect", "pending"):
            self.assertEqual(export_disagreements(answer, "none", {}), [])
            self.assertEqual(len(export_disagreements(
                answer, "none", {"ITSAppUsesNonExemptEncryption": True})), 1)
            self.assertEqual(len(export_disagreements(
                answer, "none", {"ITSAppUsesNonExemptEncryption": False})), 1)
            self.assertEqual(len(export_disagreements(
                answer, "none", {"ITSEncryptionExportComplianceCode": "abc"})), 1)

    def test_yes_needs_the_key_true_and_the_recorded_code(self) -> None:
        good = {"ITSAppUsesNonExemptEncryption": True,
                "ITSEncryptionExportComplianceCode": "abc-123"}
        self.assertEqual(export_disagreements("YES", "abc-123", good), [])
        self.assertTrue(export_disagreements("YES", "abc-123", {}))
        self.assertTrue(export_disagreements(
            "YES", "abc-123", {"ITSAppUsesNonExemptEncryption": True}))
        self.assertTrue(export_disagreements(
            "YES", "abc-123", {**good, "ITSEncryptionExportComplianceCode": "other"}))
        self.assertTrue(export_disagreements(
            "YES", "abc-123", {**good, "ITSAppUsesNonExemptEncryption": False}))
        self.assertTrue(export_disagreements("YES", "none", good))

    def test_no_needs_the_key_false(self) -> None:
        self.assertEqual(export_disagreements(
            "NO", "none", {"ITSAppUsesNonExemptEncryption": False}), [])
        self.assertTrue(export_disagreements(
            "NO", "none", {"ITSAppUsesNonExemptEncryption": True}))
        self.assertTrue(export_disagreements("NO", "none", {}))

    def test_every_cited_file_exists(self) -> None:
        text = (IOS / "AppStore" / "export-compliance.md").read_text(encoding="utf-8")
        cited = set(re.findall(r"`((?:NereusKit|NereusApp|NereusActivity|Shared)/[^`:\s]+)", text))
        self.assertTrue(cited)
        for relative in sorted(cited):
            self.assertTrue((IOS / relative).exists(), relative)


# Apple's required-reason API categories (ITMS-91053): how a use shows in the
# shipped Swift, and how it shows in a built binary (an imported symbol, or
# an Objective-C selector the binary sends).
REQUIRED_REASON_SOURCE = {
    "NSPrivacyAccessedAPICategoryUserDefaults": re.compile(r"\b(?:UserDefaults|NSUserDefaults)\b|@AppStorage\b"),
    "NSPrivacyAccessedAPICategorySystemBootTime": re.compile(r"\bsystemUptime\b|\bmach_absolute_time\b"),
    "NSPrivacyAccessedAPICategoryFileTimestamp": re.compile(
        r"\b(?:creationDate|contentModificationDate|modificationDate)Key\b|\.(?:creationDate|modificationDate)\b"
        r"|\bNSFile(?:Creation|Modification)Date\b|\b(?:f|l)?stat(?:at)?\(|\bgetattrlist"),
    "NSPrivacyAccessedAPICategoryDiskSpace": re.compile(
        r"\bvolume(?:Available|Total)Capacity|\bsystem(?:Free)?Size\b|\bNSFileSystem(?:Free)?Size\b"
        r"|\bf?statv?fs\("),
    "NSPrivacyAccessedAPICategoryActiveKeyboards": re.compile(r"\bactiveInputModes\b"),
}
REQUIRED_REASON_SYMBOLS = {
    "NSPrivacyAccessedAPICategoryUserDefaults": {"_OBJC_CLASS_$_NSUserDefaults"},
    "NSPrivacyAccessedAPICategorySystemBootTime": {"_mach_absolute_time"},
    "NSPrivacyAccessedAPICategoryFileTimestamp": {
        "_stat", "_fstat", "_lstat", "_fstatat", "_stat$INODE64", "_fstat$INODE64", "_lstat$INODE64",
        "_getattrlist", "_fgetattrlist", "_getattrlistat", "_getattrlistbulk",
        "_NSFileCreationDate", "_NSFileModificationDate", "_NSURLCreationDateKey",
        "_NSURLContentModificationDateKey", "_NSURLAttributeModificationDateKey"},
    "NSPrivacyAccessedAPICategoryDiskSpace": {
        "_statfs", "_fstatfs", "_statvfs", "_fstatvfs", "_statfs$INODE64", "_fstatfs$INODE64",
        "_NSFileSystemFreeSize", "_NSFileSystemSize", "_NSURLVolumeAvailableCapacityKey",
        "_NSURLVolumeAvailableCapacityForImportantUsageKey",
        "_NSURLVolumeAvailableCapacityForOpportunisticUsageKey", "_NSURLVolumeTotalCapacityKey"},
}
REQUIRED_REASON_SELECTORS = {
    "NSPrivacyAccessedAPICategorySystemBootTime": {"systemUptime"},
    "NSPrivacyAccessedAPICategoryActiveKeyboards": {"activeInputModes"},
}
# The reasons this app may give: Apple's approved reasons for each category
# that fit what the app does.
APPROVED_REASONS = {
    # CA92.1: information only this app reads and writes.
    "NSPrivacyAccessedAPICategoryUserDefaults": {"CA92.1"},
    # 35F9.1: time elapsed between events in the app.
    "NSPrivacyAccessedAPICategorySystemBootTime": {"35F9.1"},
    # C617.1: metadata of files the app opens itself. See VENDORED_USES.
    "NSPrivacyAccessedAPICategoryFileTimestamp": {"C617.1"},
}
# Categories the app's own Swift never uses but vendored C linked into the
# app does, with each place. The only file-metadata call in the app is
# libsodium's check that /dev/urandom is a character device (fstat, then
# S_ISCHR on st_mode); it reads no timestamp. libsodium's default random
# source on Apple is sysrandom (randombytes.c:29-35), whose arc4random
# branch is chosen only on OpenBSD, CloudABI and WASI
# (randombytes_sysrandom.c:78-80), and its configure.ac offers no switch
# to leave the /dev/urandom path out. So the manifest declares
# FileTimestamp with C617.1 and the vendored sources stay as shipped.
LIBSODIUM_RANDOMBYTES = IOS / "NereusKit" / "Sources" / "CSodium" / "src" / "libsodium" / "randombytes"
VENDORED_USES = {
    "NSPrivacyAccessedAPICategoryFileTimestamp": [
        (LIBSODIUM_RANDOMBYTES / "sysrandom" / "randombytes_sysrandom.c", 199),
        (LIBSODIUM_RANDOMBYTES / "internal" / "randombytes_internal_random.c", 308),
    ],
}
APP_MANIFEST = IOS / "NereusApp" / "Resources" / "PrivacyInfo.xcprivacy"
# What the app target ships: its own code, the shared code and NereusKit,
# linked in statically. Tests are not shipped.
SHIPPED_DIRS = ("NereusApp", "NereusActivity", "Shared", "NereusKit/Sources")
NOT_SHIPPED = {"Tests", "UITests", "NereusKitTesting"}


def used_categories(ios: Path) -> Dict[str, List[str]]:
    """Each required-reason category the shipped Swift uses, with where."""
    used: Dict[str, List[str]] = {}
    for name in SHIPPED_DIRS:
        base = ios / name
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*.swift")):
            if NOT_SHIPPED.intersection(path.relative_to(ios).parts):
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for category, pattern in REQUIRED_REASON_SOURCE.items():
                match = pattern.search(text)
                if match:
                    line = text.count("\n", 0, match.start()) + 1
                    used.setdefault(category, []).append("ios/%s:%d" % (path.relative_to(ios).as_posix(), line))
    return used


def declared_reasons(manifest: dict) -> Dict[str, Set[str]]:
    declared: Dict[str, Set[str]] = {}
    for entry in manifest.get("NSPrivacyAccessedAPITypes", []):
        declared.setdefault(entry.get("NSPrivacyAccessedAPIType", ""), set()).update(
            entry.get("NSPrivacyAccessedAPITypeReasons", []))
    return declared


def manifest_problems(manifest: dict, needed: Iterable[str]) -> List[str]:
    """What a privacy manifest gets wrong for a binary that uses `needed`."""
    problems: List[str] = []
    if manifest.get("NSPrivacyTracking") is not False:
        problems.append("NSPrivacyTracking must be false")
    if manifest.get("NSPrivacyTrackingDomains", None) != []:
        problems.append("NSPrivacyTrackingDomains must be an empty list")
    if manifest.get("NSPrivacyCollectedDataTypes", None) != []:
        problems.append("NSPrivacyCollectedDataTypes must be an empty list (Data Not Collected)")
    declared = declared_reasons(manifest)
    for category in sorted(set(needed)):
        reasons = declared.get(category, set())
        if not reasons:
            problems.append("%s is used but not declared" % category)
            continue
        allowed = APPROVED_REASONS.get(category)
        if allowed is None:
            problems.append("%s has no reason this app may give; decide one first" % category)
        elif not reasons <= allowed:
            problems.append("%s gives %s, not one of %s" % (category, sorted(reasons), sorted(allowed)))
    for category in sorted(set(declared) - set(needed)):
        problems.append("%s is declared but not used" % category)
    return problems


def binary_categories(binary: Path) -> Set[str]:
    """Each required-reason category a built Mach-O binary calls."""
    imports = set(subprocess.run(["xcrun", "nm", "-u", "-j", str(binary)], check=True,
                                 capture_output=True, text=True).stdout.split())
    selectors = set(subprocess.run(["xcrun", "otool", "-v", "-s", "__TEXT", "__objc_methname", str(binary)],
                                   check=True, capture_output=True, text=True).stdout.split())
    found: Set[str] = set()
    for category, symbols in REQUIRED_REASON_SYMBOLS.items():
        if imports & symbols:
            found.add(category)
    for category, names in REQUIRED_REASON_SELECTORS.items():
        if selectors & names:
            found.add(category)
    return found


class PrivacyManifest(unittest.TestCase):
    def test_the_app_declares_no_tracking_no_collection_and_each_api_it_uses(self) -> None:
        self.assertTrue(APP_MANIFEST.is_file(), "ios/NereusApp/Resources/PrivacyInfo.xcprivacy is missing")
        with APP_MANIFEST.open("rb") as handle:
            manifest = plistlib.load(handle)
        used = used_categories(IOS)
        self.assertIn("NSPrivacyAccessedAPICategoryUserDefaults", used)
        self.assertIn("NSPrivacyAccessedAPICategorySystemBootTime", used)
        self.assertEqual(manifest_problems(manifest, list(used) + list(VENDORED_USES)), [])

    def test_the_only_file_metadata_use_is_libsodiums_dev_urandom_check(self) -> None:
        for path, line in VENDORED_USES["NSPrivacyAccessedAPICategoryFileTimestamp"]:
            lines = path.read_text(encoding="utf-8").splitlines()
            self.assertIn("fstat(fd, &st) == 0", lines[line - 1], "%s:%d" % (path, line))
            self.assertIn("S_ISCHR(st.st_mode)", "\n".join(lines[line - 1:line + 8]), "%s:%d" % (path, line))
        self.assertNotIn("NSPrivacyAccessedAPICategoryFileTimestamp", used_categories(IOS))
        with APP_MANIFEST.open("rb") as handle:
            declared = declared_reasons(plistlib.load(handle))
        self.assertEqual(declared.get("NSPrivacyAccessedAPICategoryFileTimestamp"), {"C617.1"})

    def test_the_project_ships_it_in_the_app(self) -> None:
        project = (IOS / "project.yml").read_text(encoding="utf-8")
        app = re.search(r"^  NereusSDR:\n((?:    .*\n|\n)*)", project, re.M)
        self.assertIsNotNone(app)
        excludes = re.findall(r'^\s+- "([^"]+)"', app.group(1), re.M)
        for pattern in excludes:
            self.assertFalse(Path("NereusApp/Resources/PrivacyInfo.xcprivacy").match("NereusApp/" + pattern),
                             "the app target excludes the manifest: " + pattern)

    def test_the_checks_find_each_mistake(self) -> None:
        good = {"NSPrivacyTracking": False, "NSPrivacyTrackingDomains": [], "NSPrivacyCollectedDataTypes": [],
                "NSPrivacyAccessedAPITypes": [
                    {"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryUserDefaults",
                     "NSPrivacyAccessedAPITypeReasons": ["CA92.1"]}]}
        needed = ["NSPrivacyAccessedAPICategoryUserDefaults"]
        self.assertEqual(manifest_problems(good, needed), [])
        self.assertTrue(manifest_problems({**good, "NSPrivacyTracking": True}, needed))
        self.assertTrue(manifest_problems({**good, "NSPrivacyTrackingDomains": ["x.example"]}, needed))
        self.assertTrue(manifest_problems({**good, "NSPrivacyCollectedDataTypes": [{}]}, needed))
        self.assertTrue(manifest_problems(good, needed + ["NSPrivacyAccessedAPICategorySystemBootTime"]))
        self.assertTrue(manifest_problems(good, []))
        wrong = {**good, "NSPrivacyAccessedAPITypes": [
            {"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryUserDefaults",
             "NSPrivacyAccessedAPITypeReasons": ["1C8F.1"]}]}
        self.assertTrue(manifest_problems(wrong, needed))
        self.assertTrue(manifest_problems(good, needed + ["NSPrivacyAccessedAPICategoryDiskSpace"]))

    def test_the_source_scan_finds_each_category(self) -> None:
        cases = {
            "NereusApp/A.swift": "let d = UserDefaults.standard\n",
            "NereusKit/Sources/X/B.swift": "let t = ProcessInfo.processInfo.systemUptime\n",
            "NereusApp/C.swift": "let v = try url.resourceValues(forKeys: [.contentModificationDateKey])\n",
            "NereusApp/D.swift": "let v = try url.resourceValues(forKeys: [.volumeAvailableCapacityKey])\n",
            "NereusApp/Tests/E.swift": "let k = UITextInputMode.activeInputModes\n",
        }
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for relative, text in cases.items():
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            used = used_categories(root)
        self.assertEqual(set(used), {"NSPrivacyAccessedAPICategoryUserDefaults",
                                     "NSPrivacyAccessedAPICategorySystemBootTime",
                                     "NSPrivacyAccessedAPICategoryFileTimestamp",
                                     "NSPrivacyAccessedAPICategoryDiskSpace"})

    @unittest.skipUnless(os.environ.get("NEREUS_IOS_RELEASE_APP"),
                         "set NEREUS_IOS_RELEASE_APP to a built Release NereusSDR.app to check the bundle")
    def test_a_release_build_carries_the_manifest_each_binary_needs(self) -> None:
        app = Path(os.environ["NEREUS_IOS_RELEASE_APP"])
        self.assertTrue((app / "NereusSDR").is_file(), app)
        bundled = app / "PrivacyInfo.xcprivacy"
        self.assertTrue(bundled.is_file(), "the built app does not carry PrivacyInfo.xcprivacy")
        self.assertEqual(bundled.read_bytes(), APP_MANIFEST.read_bytes())
        with bundled.open("rb") as handle:
            self.assertEqual(manifest_problems(plistlib.load(handle), binary_categories(app / "NereusSDR")), [])
        for extension in sorted((app / "PlugIns").glob("*.appex")):
            needed = binary_categories(extension / extension.stem)
            manifest = extension / "PrivacyInfo.xcprivacy"
            if needed or manifest.exists():
                self.assertTrue(manifest.is_file(), "%s calls %s but carries no manifest" %
                                (extension.name, sorted(needed)))
                with manifest.open("rb") as handle:
                    self.assertEqual(manifest_problems(plistlib.load(handle), needed), [], extension.name)


class AboutLinks(unittest.TestCase):
    def setUp(self) -> None:
        self.content = json.loads(
            (IOS / "NereusApp" / "Resources" / "AboutContent.json").read_text(encoding="utf-8"))

    def test_source_and_privacy_point_at_the_website_pages(self) -> None:
        self.assertEqual(self.content["sourceLink"],
                         {"title": "Corresponding Source", "url": "https://nereussdr.com/iphone-source"})
        self.assertEqual(self.content["privacyLink"],
                         {"title": "Privacy", "url": "https://nereussdr.com/iphone-privacy"})

    def test_the_pages_exist_and_are_in_the_sitemap(self) -> None:
        sitemap = (SITE / "sitemap.xml").read_text(encoding="utf-8")
        for key in ("sourceLink", "privacyLink"):
            url = self.content[key]["url"]
            page = site_page(url)
            self.assertTrue(page.is_file(), url)
            html = page.read_text(encoding="utf-8")
            self.assertIn('<link rel="canonical" href="%s">' % url, html)
            self.assertIn("<loc>%s</loc>" % url, sitemap)
            self.assertNotIn("\u2014", html)

    def test_the_store_listing_links_the_same_pages(self) -> None:
        listing = (IOS / "AppStore" / "description.md").read_text(encoding="utf-8")
        for key in ("sourceLink", "privacyLink"):
            self.assertIn(self.content[key]["url"], listing)


if __name__ == "__main__":
    unittest.main()
