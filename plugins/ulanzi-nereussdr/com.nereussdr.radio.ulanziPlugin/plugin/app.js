// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
//                AI-assisted implementation via Anthropic Claude Code.
//
// NereusSDR for Ulanzi Studio. Written for NereusSDR after reading how the AetherSDR plugin
// talks to the host; none of its code is here, and this plugin calls no TCI. The host side is
// Ulanzi's SDK in ../libs/common-node, an ES module, so it comes in through import().
'use strict';

const fs = require('fs');
const path = require('path');
const { DeckClient } = require('./link.js');

const kPluginUuid = 'com.nereussdr.radio.controller';
const kOnImage = fs.readFileSync(path.join(__dirname, '..', 'assets', 'icons', 'on.png')).toString('base64');

const link = new DeckClient({ plugin: 'ulanzi', version: '1.0.0' });

// jsn.uuid carries the action class, and each event has the context the SDK made for its key.
const contexts = new Map();     // context: { id, slice }
const held = new Map();         // context: action id, while down
const lit = new Set();
let display = null;

function idOf(uuid) {
    return String(uuid).slice(kPluginUuid.length + 1);
}

function remember(jsn) {
    const param = jsn.param || {};
    contexts.set(jsn.context, { id: idOf(jsn.uuid), slice: param.slice });
}

function textFor(id) {
    return id.startsWith('tune.') && display ? display.frequency : '';
}

function paint($UD, context, entry) {
    if (lit.has(entry.id)) {
        $UD.setBaseDataIcon(context, kOnImage, textFor(entry.id));
    } else {
        $UD.setStateIcon(context, 0, textFor(entry.id));
    }
}

function down(jsn) {
    const entry = contexts.get(jsn.context);
    if (entry) {
        held.set(jsn.context, entry.id);
        link.keyDown(entry.id, entry.slice);
    }
}

function up(jsn) {
    const entry = contexts.get(jsn.context);
    if (entry) {
        held.delete(jsn.context);
        link.keyUp(entry.id);
    }
}

function turn(jsn, ticks) {
    const entry = contexts.get(jsn.context);
    if (entry) {
        link.dialTicks(entry.id, ticks, entry.slice);
    }
}

async function main() {
    const { default: UlanziApi } = await import('../libs/common-node/index.js');
    const $UD = new UlanziApi();
    $UD.connect(kPluginUuid);

    $UD.onConnected(() => link.start());
    // Must leave when the host goes: nothing else would ever let go of a held key, and a
    // process that stayed would keep its socket open. Leaving drops it, and holds release.
    $UD.onClose(() => process.exit(0));
    $UD.onError(() => {});      // the close that follows ends this

    $UD.onAdd((jsn) => {
        remember(jsn);
        paint($UD, jsn.context, contexts.get(jsn.context));
    });
    $UD.onParamFromApp(remember);
    $UD.onParamFromPlugin(remember);
    $UD.onClear((jsn) => {
        for (const item of jsn.param || []) {
            const id = held.get(item.context);
            if (id !== undefined) {
                held.delete(item.context);
                link.keyUp(id);
            }
            contexts.delete(item.context);
        }
    });

    $UD.onKeyDown(down);
    $UD.onKeyUp(up);
    $UD.onDialDown(down);
    $UD.onDialUp(up);
    $UD.onDialRotateRight((jsn) => turn(jsn, 1));
    $UD.onDialRotateLeft((jsn) => turn(jsn, -1));

    link.on('state', (id, on) => {
        if (on === lit.has(id)) {
            return;
        }
        if (on) {
            lit.add(id);
        } else {
            lit.delete(id);
        }
        for (const [context, entry] of contexts) {
            if (entry.id === id) {
                paint($UD, context, entry);
            }
        }
    });
    link.on('display', (next) => {
        display = next;
        for (const [context, entry] of contexts) {
            if (entry.id.startsWith('tune.')) {
                paint($UD, context, entry);
            }
        }
    });
    link.on('disconnected', () => {
        held.clear();
        display = null;
        lit.clear();
        for (const [context, entry] of contexts) {
            paint($UD, context, entry);
        }
    });
}

// A failure here ends the process, which is what a plugin with no host should do.
main().catch((error) => {
    console.error(error);
    process.exit(1);
});
