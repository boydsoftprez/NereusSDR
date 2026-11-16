// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
//                AI-assisted implementation via Anthropic Claude Code.
//
// The plugin's end of the deck link to the NereusSDR window: a local socket carrying one
// JSON object per line. The Elgato and Ulanzi plugins carry this same file, byte for byte;
// plugins/tools/gen-manifests.py --check holds them together.
//
// A press made while no window answers is dropped, never queued: a hold must not start
// later on its own. When this process ends, the socket drops and the window releases every
// hold this plugin had.
'use strict';

const EventEmitter = require('events');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');

const kProtocol = 1;
const kRetryMs = 2000;
const kAnswerMs = 2000;     // how long a socket may take to answer a hello
const kMaxLineBytes = 65536; // as DeckLink::kMaxLineBytes

function deckFolder(home) {
    return path.join(home, '.nereussdr', 'deck');
}

// Every discovery file that can be read, newest first.
function discover(home) {
    const folder = deckFolder(home);
    let names;
    try {
        names = fs.readdirSync(folder);
    } catch {
        return [];
    }
    const found = [];
    for (const name of names) {
        if (!name.endsWith('.json')) {
            continue;
        }
        const file = path.join(folder, name);
        try {
            const entry = JSON.parse(fs.readFileSync(file, 'utf8'));
            if (entry.protocol === kProtocol && typeof entry.path === 'string') {
                found.push({ socketPath: entry.path, mtimeMs: fs.statSync(file).mtimeMs });
            }
        } catch {
            // Left half written, or gone since the listing: not one to try.
        }
    }
    found.sort((a, b) => b.mtimeMs - a.mtimeMs);
    return found.map((entry) => entry.socketPath);
}

// tune.up and tune.down, tune.up10 and tune.down10, volume.up and volume.down. A dial turns
// a pair, whichever of the two the action on it names. Null for anything else.
function turnPair(id) {
    const match = /^(.*\.)(up|down)(\d*)$/.exec(id);
    if (!match) {
        return null;
    }
    return { idUp: `${match[1]}up${match[3]}`, idDown: `${match[1]}down${match[3]}` };
}

// A slice the link accepts, or nothing: with no slice chosen, pin must be left out.
function validPin(pin) {
    return typeof pin === 'string' && /^[A-D]$/.test(pin) ? pin : undefined;
}

class DeckClient extends EventEmitter {
    constructor({ plugin, version, home = os.homedir(), retryMs = kRetryMs, answerMs = kAnswerMs }) {
        super();
        this.plugin = plugin;
        this.version = version;
        this.home = home;
        this.retryMs = retryMs;
        this.answerMs = answerMs;
        this.socket = null;     // set only once a window has answered
        this.attempt = null;
        this.retryTimer = null;
        this.stopped = true;
    }

    start() {
        if (this.stopped) {
            this.stopped = false;
            this.tryAll();
        }
    }

    stop() {
        this.stopped = true;
        clearTimeout(this.retryTimer);
        if (this.attempt) {
            this.attempt.destroy();
        }
        if (this.socket) {
            this.socket.destroy();
        }
    }

    connected() {
        return this.socket !== null;
    }

    keyDown(id, pin) {
        this.send({ type: 'press', id, pin: validPin(pin) });
    }

    keyUp(id) {
        this.send({ type: 'release', id });
    }

    dialTicks(id, ticks, pin) {
        const pair = turnPair(id);
        const steps = Math.trunc(Number(ticks));
        if (pair && steps !== 0 && Number.isFinite(steps)) {
            this.send({ type: 'turn', idUp: pair.idUp, idDown: pair.idDown, steps, pin: validPin(pin) });
        }
    }

    send(message) {
        if (this.socket) {
            this.socket.write(`${JSON.stringify(message)}\n`);
        }
    }

    tryAll() {
        this.tryNext(discover(this.home));
    }

    tryNext(paths) {
        if (this.stopped) {
            return;
        }
        if (paths.length === 0) {
            this.retryTimer = setTimeout(() => this.tryAll(), this.retryMs);
            return;
        }
        const [socketPath, ...rest] = paths;
        const socket = net.connect({ path: socketPath });
        this.attempt = socket;
        let buffer = Buffer.alloc(0);
        let answered = false;

        const noAnswer = setTimeout(() => socket.destroy(), this.answerMs);
        socket.on('connect', () => {
            socket.write(`${JSON.stringify({ type: 'hello', plugin: this.plugin, version: this.version })}\n`);
        });
        socket.on('data', (chunk) => {
            buffer = Buffer.concat([buffer, chunk]);
            let end;
            while ((end = buffer.indexOf(0x0a)) >= 0) {
                const line = buffer.subarray(0, end).toString('utf8');
                buffer = buffer.subarray(end + 1);
                let message;
                try {
                    message = JSON.parse(line);
                } catch {
                    continue;
                }
                if (!answered) {
                    if (message.type !== 'hello' || message.protocol !== kProtocol) {
                        socket.destroy();
                        return;
                    }
                    answered = true;
                    clearTimeout(noAnswer);
                    this.attempt = null;
                    this.socket = socket;
                    this.emit('connected', message.actions || []);
                } else if (message.type === 'state') {
                    this.emit('state', message.id, message.on === true);
                } else if (message.type === 'display') {
                    this.emit('display', {
                        frequency: message.frequency,
                        mode: message.mode,
                        filter: message.filter,
                        slice: message.slice,
                    });
                }
            }
            if (buffer.length > kMaxLineBytes) {
                socket.destroy();
            }
        });
        socket.on('error', () => {});   // 'close' follows
        socket.on('close', () => {
            clearTimeout(noAnswer);
            if (answered) {
                this.socket = null;
                this.emit('disconnected');
                if (!this.stopped) {
                    this.retryTimer = setTimeout(() => this.tryAll(), this.retryMs);
                }
            } else {
                this.attempt = null;
                this.tryNext(rest);
            }
        });
    }
}

module.exports = { DeckClient, discover, deckFolder, turnPair, validPin, kRetryMs };
