# Deck plugins

Plugins that run NereusSDR's hot key actions from Elgato's Stream Deck app, Ulanzi Studio and
StreamController. Each talks only to the NereusSDR window on the same computer, over the deck
link (a local socket, `docs/architecture/2026-11-05-hot-keys-design.md` section 7.5).

**All three are untested.** Their clients are checked against the real socket; no plugin host
has run them, and those hosts' sandboxes aren't documented.

| Folder | Host | Hello name |
|---|---|---|
| `elgato-nereussdr/com.nereussdr.radio.sdPlugin` | Elgato Stream Deck app | `elgato` |
| `ulanzi-nereussdr/com.nereussdr.radio.ulanziPlugin` | Ulanzi Studio | `ulanzi` |
| `streamcontroller-nereussdr` | StreamController (Linux) | `streamcontroller` |

## When the action list changes

`tst_deck_plugin_actions` fails until these run:

```
NEREUS_WRITE_ACTIONS_JSON=1 ./build/tests/tst_deck_plugin_actions
python3 plugins/tools/gen-manifests.py
```

`link.js` is edited in the Elgato folder; `gen-manifests.py` copies it to the Ulanzi folder.

## Where they end up

The `NereusDeckPlugins` target lays them out in `<build>/deck-plugins`, with the Elgato folder
zipped to `NereusSDR.streamDeckPlugin`. They ship in the app's `Contents/Resources/plugins` on
macOS, `plugins/` beside the executable on Windows, and `share/nereussdr/plugins` on Linux.

## Tests

```
node plugins/elgato-nereussdr/link.test.js
python3 plugins/streamcontroller-nereussdr/test_link.py
```
