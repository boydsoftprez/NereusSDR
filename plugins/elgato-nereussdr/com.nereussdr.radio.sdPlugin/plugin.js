// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
//                AI-assisted implementation via Anthropic Claude Code.
//
// NereusSDR for Elgato's Stream Deck app. Written for NereusSDR after reading how the
// AetherSDR plugin talks to the host; none of its code is here, and this plugin calls no TCI.
// Host events and commands are from https://docs.elgato.com/streamdeck/sdk/references/websocket/plugin
// and the launch arguments as AetherSDR plugins/elgato-aethersdr/.../plugin.js:9-16 [@1e0718ad].
'use strict';

const WebSocket = require('ws');
const { DeckClient } = require('./link.js');

const kPluginUuid = 'com.nereussdr.radio';
const kOnImage = 'imgs/key-on.png';

function argument(name) {
    const at = process.argv.indexOf(name);
    return at >= 0 ? process.argv[at + 1] : undefined;
}

const host = new WebSocket(`ws://127.0.0.1:${argument('-port')}`);
const link = new DeckClient({ plugin: 'elgato', version: '1.0.0' });

const contexts = new Map();     // context: { id, controller, device }
const held = new Map();         // context: action id, while its key or dial is down
const lit = new Set();
let display = null;

function hostSend(message) {
    if (host.readyState === WebSocket.OPEN) {
        host.send(JSON.stringify(message));
    }
}

function actionId(uuid) {
    return String(uuid).slice(kPluginUuid.length + 1);
}

function slicePin(message) {
    const settings = message.payload && message.payload.settings;
    return settings ? settings.slice : undefined;
}

function down(message) {
    const id = actionId(message.action);
    held.set(message.context, id);
    link.keyDown(id, slicePin(message));
}

function up(message) {
    held.delete(message.context);
    link.keyUp(actionId(message.action));
}

// A key that leaves the screen or the desk while down will never say it went up.
function letGo(context) {
    const id = held.get(context);
    if (id !== undefined) {
        held.delete(context);
        link.keyUp(id);
    }
}

function paintLight(context, id) {
    const payload = { target: 0 };
    if (lit.has(id)) {
        payload.image = kOnImage;
    }
    hostSend({ event: 'setImage', context, payload });
}

function paintDial(context, id) {
    if (!id.startsWith('tune.')) {
        return;
    }
    const slice = display && display.slice ? `Slice ${display.slice}` : 'NereusSDR';
    hostSend({ event: 'setFeedback', context, payload: { title: slice, value: display ? display.frequency : '' } });
}

host.on('open', () => {
    hostSend({ event: argument('-registerEvent'), uuid: argument('-pluginUUID') });
    link.start();
});

host.on('message', (data) => {
    let message;
    try {
        message = JSON.parse(data.toString());
    } catch {
        return;
    }
    switch (message.event) {
    case 'willAppear': {
        const id = actionId(message.action);
        const controller = message.payload ? message.payload.controller : 'Keypad';
        contexts.set(message.context, { id, controller, device: message.device });
        if (controller === 'Encoder') {
            paintDial(message.context, id);
        } else if (lit.has(id)) {
            paintLight(message.context, id);
        }
        break;
    }
    case 'willDisappear':
        letGo(message.context);
        contexts.delete(message.context);
        break;
    case 'deviceDidDisconnect':
        for (const [context, entry] of contexts) {
            if (entry.device === message.device) {
                letGo(context);
            }
        }
        break;
    case 'keyDown':
    case 'dialDown':
        down(message);
        break;
    case 'keyUp':
    case 'dialUp':
        up(message);
        break;
    case 'dialRotate':
        link.dialTicks(actionId(message.action), message.payload.ticks, slicePin(message));
        break;
    default:
        break;
    }
});

// The host starts this process and owns its life. With it gone no key can ever come up, so
// leave: the socket drops and the window releases what this plugin held.
host.on('close', () => process.exit(0));
host.on('error', () => process.exit(1));

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
        if (entry.id === id && entry.controller !== 'Encoder') {
            paintLight(context, id);
        }
    }
});

link.on('display', (next) => {
    display = next;
    for (const [context, entry] of contexts) {
        if (entry.controller === 'Encoder') {
            paintDial(context, entry.id);
        }
    }
});

link.on('disconnected', () => {
    // The window has already let go of everything this connection held.
    held.clear();
    display = null;
    const wereLit = [...lit];
    lit.clear();
    for (const [context, entry] of contexts) {
        if (entry.controller === 'Encoder') {
            paintDial(context, entry.id);
        } else if (wereLit.includes(entry.id)) {
            paintLight(context, entry.id);
        }
    }
});
