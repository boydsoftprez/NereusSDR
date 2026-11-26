#!/usr/bin/env python3
# NereusSDR for iOS: checks a build number against App Store Connect and hands an uploaded build to a TestFlight group
# SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
"""App Store Connect steps of .github/workflows/ios-testflight.yml.

Usage:
  testflight.py check      --bundle-id ID --version 2026.11.1 --build 10300
  testflight.py distribute --bundle-id ID --version 2026.11.1 --build 10300 --group Me

check       Stops (exit 1) when App Store Connect already holds a build of
            this version with this number or a higher one: an upload with
            it would be refused after the archive, so this runs first.
distribute  Waits for the uploaded build to finish processing, then adds it
            to the TestFlight group. An external group (testers outside the
            team) also needs Beta App Review: the build is sent for it, and
            when App Store Connect refuses that (most often because the
            export compliance answer is not given yet), the refusal is
            printed and the run still succeeds, since the upload itself
            worked.

Export compliance is never answered here. JJ answers it in App Store
Connect for each build (ios/AppStore/export-compliance.md); until then
testers see the build as "Missing Compliance" and cannot install it.

Credentials come from the environment: ASC_KEY_ID, ASC_ISSUER_ID and
ASC_KEY_PATH (the .p8 file). Needs PyJWT and cryptography.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from typing import Any, Dict, List, Optional, Tuple

import jwt  # PyJWT

API = "https://api.appstoreconnect.apple.com"

# App Store Connect limits a token to 20 minutes.
kTokenLifetimeSeconds = 19 * 60
kPollSeconds = 30


class Api:
    def __init__(self) -> None:
        missing = [n for n in ("ASC_KEY_ID", "ASC_ISSUER_ID", "ASC_KEY_PATH") if not os.environ.get(n)]
        if missing:
            fail("missing environment: " + ", ".join(missing))
        self.key_id = os.environ["ASC_KEY_ID"]
        self.issuer = os.environ["ASC_ISSUER_ID"]
        with open(os.environ["ASC_KEY_PATH"], "r", encoding="utf-8") as f:
            self.key = f.read()
        self.token = ""
        self.token_expires = 0.0

    def _token(self) -> str:
        now = time.time()
        if now > self.token_expires - 60:
            self.token = jwt.encode(
                {"iss": self.issuer, "iat": int(now), "exp": int(now + kTokenLifetimeSeconds),
                 "aud": "appstoreconnect-v1"},
                self.key, algorithm="ES256", headers={"kid": self.key_id, "typ": "JWT"})
            self.token_expires = now + kTokenLifetimeSeconds
        return self.token

    def request(self, method: str, path: str, params: Optional[Dict[str, str]] = None,
                body: Optional[Dict[str, Any]] = None) -> Tuple[int, Dict[str, Any]]:
        url = API + path
        if params:
            url += "?" + urllib.parse.urlencode(params)
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(url, data=data, method=method)
        req.add_header("Authorization", "Bearer " + self._token())
        if data is not None:
            req.add_header("Content-Type", "application/json")
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                raw = resp.read()
                return resp.status, (json.loads(raw) if raw else {})
        except urllib.error.HTTPError as e:
            raw = e.read()
            try:
                return e.code, json.loads(raw)
            except ValueError:
                return e.code, {"errors": [{"detail": raw.decode(errors="replace")}]}

    def get(self, path: str, params: Optional[Dict[str, str]] = None) -> Dict[str, Any]:
        status, doc = self.request("GET", path, params)
        if status != 200:
            fail(f"GET {path} answered {status}: {errors(doc)}")
        return doc


def fail(message: str) -> None:
    print(f"::error title=TestFlight::{message}")
    sys.exit(1)


def errors(doc: Dict[str, Any]) -> str:
    return "; ".join(
        f"{e.get('title', '')} {e.get('detail', '')}".strip() for e in doc.get("errors", [])) or json.dumps(doc)


def app_id(api: Api, bundle_id: str) -> str:
    doc = api.get("/v1/apps", {"filter[bundleId]": bundle_id, "fields[apps]": "bundleId"})
    for app in doc.get("data", []):
        if app["attributes"]["bundleId"] == bundle_id:
            return app["id"]
    fail(f"App Store Connect has no app with bundle ID {bundle_id}; check the API key's team.")
    return ""


def builds(api: Api, app: str, version: Optional[str] = None) -> List[Dict[str, Any]]:
    params = {"filter[app]": app, "limit": "200",
              "fields[builds]": "version,processingState,uploadedDate,usesNonExemptEncryption"}
    if version:
        params["filter[preReleaseVersion.version]"] = version
    return api.get("/v1/builds", params).get("data", [])


def number(build: Dict[str, Any]) -> int:
    try:
        return int(build["attributes"]["version"])
    except (KeyError, ValueError):
        return -1


def check(api: Api, args: argparse.Namespace) -> None:
    app = app_id(api, args.bundle_id)
    same_version = builds(api, app, args.version)
    highest = max((number(b) for b in same_version), default=None)
    every = builds(api, app)
    highest_any = max((number(b) for b in every), default=None)
    print(f"App Store Connect: highest build of {args.version} is {highest}, "
          f"highest of any version (newest 200) is {highest_any}; this build is {args.build}.")
    if highest is not None and highest >= args.build:
        fail(f"build {args.build} is not above {highest}, already uploaded for {args.version}; "
             "App Store Connect would refuse it. Raise kBuildNumberOffset in "
             "ios/scripts/build-number.sh, as on 2026-10-31, or archive a newer commit.")
    if highest_any is not None and highest_any >= args.build:
        print(f"::warning title=TestFlight::build {args.build} sorts below build {highest_any} "
              "of another version in TestFlight.")


def wait_for_build(api: Api, app: str, args: argparse.Namespace) -> Dict[str, Any]:
    deadline = time.time() + args.timeout_minutes * 60
    last_state = None
    while time.time() < deadline:
        for b in builds(api, app, args.version):
            if number(b) == args.build:
                state = b["attributes"].get("processingState")
                if state != last_state:
                    print(f"Build {args.build}: {state}")
                    last_state = state
                if state == "VALID":
                    return b
                if state in ("FAILED", "INVALID"):
                    fail(f"App Store Connect could not process build {args.build} ({state}); "
                         "Apple's email to the account holder says why.")
                break
        else:
            if last_state is None:
                print(f"Build {args.build} is not in App Store Connect yet; waiting.")
                last_state = "absent"
        time.sleep(kPollSeconds)
    fail(f"build {args.build} did not finish processing in {args.timeout_minutes} minutes; "
         "add it to the group by hand in App Store Connect once it does.")
    return {}


def group_for(api: Api, app: str, name: str) -> Dict[str, Any]:
    doc = api.get("/v1/betaGroups", {
        "filter[app]": app, "limit": "200",
        "fields[betaGroups]": "name,isInternalGroup,hasAccessToAllBuilds"})
    names = []
    for g in doc.get("data", []):
        names.append(g["attributes"]["name"])
        if g["attributes"]["name"] == name:
            return g
    fail(f"no TestFlight group named '{name}'; the app's groups are: {', '.join(names) or 'none'}.")
    return {}


def distribute(api: Api, args: argparse.Namespace) -> None:
    app = app_id(api, args.bundle_id)
    group = group_for(api, app, args.group)
    build = wait_for_build(api, app, args)
    build_id = build["id"]
    attrs = group["attributes"]
    internal = bool(attrs.get("isInternalGroup"))

    if internal and attrs.get("hasAccessToAllBuilds"):
        print(f"'{args.group}' is an internal group that gets every build; nothing to add.")
    else:
        status, doc = api.request("POST", f"/v1/betaGroups/{group['id']}/relationships/builds",
                                  body={"data": [{"type": "builds", "id": build_id}]})
        if status not in (200, 201, 204):
            fail(f"could not add build {args.build} to '{args.group}' ({status}): {errors(doc)}")
        print(f"Added build {args.build} to '{args.group}'.")

    if not internal:
        status, doc = api.request("POST", "/v1/betaAppReviewSubmissions", body={
            "data": {"type": "betaAppReviewSubmissions",
                     "relationships": {"build": {"data": {"type": "builds", "id": build_id}}}}})
        if status in (200, 201):
            print(f"Sent build {args.build} to Beta App Review for '{args.group}'.")
        else:
            print(f"::warning title=TestFlight::Beta App Review refused build {args.build} ({status}): "
                  f"{errors(doc)}. Answer export compliance in App Store Connect, then submit it there.")

    if build["attributes"].get("usesNonExemptEncryption") is None:
        print(f"::notice title=TestFlight::Build {args.build} needs its export compliance answer in "
              "App Store Connect (TestFlight, the build, Manage) before testers can install it: "
              "YES, per ios/AppStore/export-compliance.md.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("check", "distribute"):
        p = sub.add_parser(name)
        p.add_argument("--bundle-id", required=True)
        p.add_argument("--version", required=True)
        p.add_argument("--build", required=True, type=int)
        if name == "distribute":
            p.add_argument("--group", required=True)
            p.add_argument("--timeout-minutes", type=int, default=60)
    args = parser.parse_args()
    api = Api()
    if args.command == "check":
        check(api, args)
    else:
        distribute(api, args)


if __name__ == "__main__":
    main()
