# Qt compatibility references

| NereusSDR file | Qt source/version | Kind and modification |
| --- | --- | --- |
| src/gui/QtCocoaAccessibilityOwnershipGuard.mm | qtbase v6.11.0 src/plugins/platforms/cocoa/qcocoaaccessibilityelement.mm:90–105,219–226,257–268,312–340 | App-local wrapper: preserve parent-owned synthetic IDs while forwarding original Qt method bodies. Exact Qt source header retained. Objective-C runtime metadata and plugin UUID were verified against the crashing installed arm64 binary. No Qt method body is copied. JJ Boyd (KG4VCF), 2026-10-03, with OpenAI Codex assistance. |

This uses private Qt class/ivar ABI inside the GUI process. It is limited to
the proven Qt6.11.0 Cocoa image, requires explicit startup refusal if that
targeted image/ABI has drifted or another interceptor owns the methods, and
must be removed when the Qt dependency carries the upstream ownership fix.
Unrelated versions/platforms receive no method changes. No global Qt files are
patched and no accessibility behavior or setting is disabled.
