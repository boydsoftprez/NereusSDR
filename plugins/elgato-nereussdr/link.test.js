// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
//                AI-assisted implementation via Anthropic Claude Code.
//
// node link.test.js                      link.js and both Node hosts' shells, against stubs
// node link.test.js --integration <home> a client tst_deck_plugin_clients drives through stdin
'use strict';

const assert = require('assert');
const { spawn } = require('child_process');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');
const readline = require('readline');

const kFolder = path.join(__dirname, 'com.nereussdr.radio.sdPlugin');
const kUlanziFolder = path.join(__dirname, '..', 'ulanzi-nereussdr', 'com.nereussdr.radio.ulanziPlugin');
const { DeckClient, deckFolder, turnPair, validPin, kRetryMs } = require(path.join(kFolder, 'link.js'));
const WebSocket = require(path.join(kFolder, 'node_modules', 'ws'));

const kWaitMs = 30000;

function integration(home) {
    const client = new DeckClient({ plugin: 'elgato', version: 'test', home });
    // Reads its orders only once connected: a press before that is dropped, by design.
    client.once('connected', () => {
        readline.createInterface({ input: process.stdin }).on('line', (line) => {
            const [verb, id, ticks] = line.split(' ');
            if (verb === 'press') {
                client.keyDown(id);
            } else if (verb === 'release') {
                client.keyUp(id);
            } else if (verb === 'turn') {
                client.dialTicks(id, Number(ticks));
            }
        });
    });
    client.start();
}

// ---- stubs ----

function scratchHome() {
    // /tmp, not the system's temp folder: a socket's path has a short limit.
    return fs.mkdtempSync(process.platform === 'win32' ? path.join(os.tmpdir(), 'ltj-') : '/tmp/ltj-');
}

let fileAge = 0;

function writeDiscovery(home, name, socketPath, { ageSeconds, protocol = 1 } = {}) {
    fs.mkdirSync(deckFolder(home), { recursive: true });
    const file = path.join(deckFolder(home), `${name}.json`);
    fs.writeFileSync(file, JSON.stringify({ protocol, path: socketPath, pid: 1, profile: '' }));
    const when = new Date(Date.now() - (ageSeconds === undefined ? ++fileAge : ageSeconds) * 1000);
    fs.utimesSync(file, when, when);
}

// A stub of the window. Answers each hello, and keeps every line it hears.
class StubWindow {
    constructor(socketPath, { answer = true, helloChunks = null } = {}) {
        this.lines = [];
        this.sockets = new Set();
        this.waiters = [];
        this.server = net.createServer((socket) => {
            this.sockets.add(socket);
            socket.on('close', () => {
                this.sockets.delete(socket);
                this.poke();
            });
            socket.on('error', () => {});
            let buffer = '';
            socket.on('data', (chunk) => {
                buffer += chunk.toString();
                let end;
                while ((end = buffer.indexOf('\n')) >= 0) {
                    const message = JSON.parse(buffer.slice(0, end));
                    buffer = buffer.slice(end + 1);
                    this.lines.push(message);
                    if (message.type === 'hello' && answer) {
                        const text = `${JSON.stringify({ type: 'hello', protocol: 1, actions: [{ id: 'x' }] })}\n`;
                        if (helloChunks) {
                            // Half a line now, the rest later: a client must wait for the newline.
                            socket.write(text.slice(0, helloChunks));
                            setTimeout(() => socket.write(text.slice(helloChunks)), 20);
                        } else {
                            socket.write(text);
                        }
                    }
                    this.poke();
                }
            });
        });
        this.listening = new Promise((resolve) => this.server.listen(socketPath, resolve));
    }

    poke() {
        for (const check of this.waiters.splice(0)) {
            check();
        }
    }

    until(condition, what) {
        return until(condition, what, (check) => this.waiters.push(check));
    }

    heard(type, count = 1) {
        return this.until(() => this.lines.filter((l) => l.type === type).length >= count, `${count} ${type}`)
            .then(() => this.lines.filter((l) => l.type === type));
    }

    broadcast(text) {
        for (const socket of this.sockets) {
            socket.write(text);
        }
    }

    close() {
        for (const socket of this.sockets) {
            socket.destroy();
        }
        return new Promise((resolve) => this.server.close(resolve));
    }
}

function until(condition, what, subscribe) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
            clearInterval(poll);
            reject(new Error(`Timed out waiting for ${what}`));
        }, kWaitMs);
        const poll = setInterval(check, 10);
        function check() {
            if (condition()) {
                clearTimeout(timer);
                clearInterval(poll);
                resolve();
            } else if (subscribe) {
                subscribe(check);
            }
        }
        check();
    });
}

function once(emitter, event) {
    return new Promise((resolve) => emitter.once(event, (...args) => resolve(args)));
}

function client(home, options = {}) {
    return new DeckClient({ plugin: 'elgato', version: '9', home, retryMs: 20, answerMs: 200, ...options });
}

// ---- link.js ----

const tests = {
    async 'retries every two seconds by default'() {
        assert.strictEqual(kRetryMs, 2000);
    },

    async 'says hello, with its name and version'() {
        const home = scratchHome();
        const window = new StubWindow(path.join(home, 'a.sock'));
        await window.listening;
        writeDiscovery(home, 'a', path.join(home, 'a.sock'));
        const c = client(home);
        c.start();
        await once(c, 'connected');
        assert.deepStrictEqual(window.lines[0], { type: 'hello', plugin: 'elgato', version: '9' });
        c.stop();
        await window.close();
    },

    async 'tries the newest file first and skips what does not answer'() {
        const home = scratchHome();
        const silent = new StubWindow(path.join(home, 's.sock'), { answer: false });
        const good = new StubWindow(path.join(home, 'g.sock'));
        const older = new StubWindow(path.join(home, 'o.sock'));
        await Promise.all([silent.listening, good.listening, older.listening]);
        writeDiscovery(home, 'dead', path.join(home, 'nothing-here.sock'), { ageSeconds: 1 });
        writeDiscovery(home, 'silent', path.join(home, 's.sock'), { ageSeconds: 2 });
        writeDiscovery(home, 'other', path.join(home, 'x.sock'), { ageSeconds: 3, protocol: 99 });
        writeDiscovery(home, 'good', path.join(home, 'g.sock'), { ageSeconds: 4 });
        writeDiscovery(home, 'older', path.join(home, 'o.sock'), { ageSeconds: 5 });
        fs.writeFileSync(path.join(deckFolder(home), 'broken.json'), '{half');
        fs.writeFileSync(path.join(deckFolder(home), 'g.sock.tmp'), 'not one');
        const c = client(home);
        c.start();
        await once(c, 'connected');
        assert.strictEqual(silent.lines.length, 1, 'the silent one was asked');
        assert.strictEqual(good.lines.length, 1);
        assert.strictEqual(older.lines.length, 0, 'older not reached');
        c.stop();
        await Promise.all([silent.close(), good.close(), older.close()]);
    },

    async 'retries until a window comes, and again when it goes'() {
        const home = scratchHome();
        const socketPath = path.join(home, 'r.sock');
        const c = client(home);
        c.start();          // no folder at all yet
        await new Promise((resolve) => setTimeout(resolve, 60));
        assert.strictEqual(c.connected(), false);
        const first = new StubWindow(socketPath);
        await first.listening;
        writeDiscovery(home, 'r', socketPath);
        await once(c, 'connected');
        const gone = once(c, 'disconnected');
        await first.close();
        await gone;
        assert.strictEqual(c.connected(), false);
        const back = once(c, 'connected');
        const second = new StubWindow(socketPath);
        await second.listening;
        await back;
        c.stop();
        await second.close();
    },

    async 'buffers partial lines, the answer and what follows'() {
        const home = scratchHome();
        const window = new StubWindow(path.join(home, 'p.sock'), { helloChunks: 5 });
        await window.listening;
        writeDiscovery(home, 'p', path.join(home, 'p.sock'));
        const c = client(home);
        const states = [];
        c.on('state', (id, on) => states.push([id, on]));
        c.start();
        const [actions] = await once(c, 'connected');
        assert.deepStrictEqual(actions, [{ id: 'x' }]);
        window.broadcast('{"type":"state","id":"tx.m');
        await new Promise((resolve) => setTimeout(resolve, 20));
        assert.deepStrictEqual(states, []);
        window.broadcast('ox","on":true}\n{"type":"state","id":"mute","on":false}\n{"not json\n');
        await until(() => states.length === 2, 'two states');
        assert.deepStrictEqual(states, [['tx.mox', true], ['mute', false]]);
        c.stop();
        await window.close();
    },

    async 'surfaces the display'() {
        const home = scratchHome();
        const window = new StubWindow(path.join(home, 'd.sock'));
        await window.listening;
        writeDiscovery(home, 'd', path.join(home, 'd.sock'));
        const c = client(home);
        c.start();
        await once(c, 'connected');
        const display = once(c, 'display');
        window.broadcast('{"type":"display","frequency":"14.250.000","mode":"USB","filter":"2.7K","slice":"A"}\n');
        assert.deepStrictEqual((await display)[0],
            { frequency: '14.250.000', mode: 'USB', filter: '2.7K', slice: 'A' });
        c.stop();
        await window.close();
    },

    async 'turns keys and ticks into press, release and turn'() {
        const home = scratchHome();
        const window = new StubWindow(path.join(home, 'k.sock'));
        await window.listening;
        writeDiscovery(home, 'k', path.join(home, 'k.sock'));
        const c = client(home);
        c.keyDown('tx.hold');   // nothing to say it to, and it must not wait for one
        c.start();
        await once(c, 'connected');
        c.keyDown('tx.hold');
        c.keyDown('mute', 'B');
        c.keyDown('mute', '');  // no slice chosen: a pin of nothing is refused, so none is sent
        c.keyUp('tx.hold');
        c.dialTicks('tune.up', 3);
        c.dialTicks('tune.down', -2, 'C');
        c.dialTicks('tune.up', 0);
        c.dialTicks('mute', 1);
        await window.heard('turn', 2);
        assert.deepStrictEqual(window.lines.slice(1), [
            { type: 'press', id: 'tx.hold' },
            { type: 'press', id: 'mute', pin: 'B' },
            { type: 'press', id: 'mute' },
            { type: 'release', id: 'tx.hold' },
            { type: 'turn', idUp: 'tune.up', idDown: 'tune.down', steps: 3 },
            { type: 'turn', idUp: 'tune.up', idDown: 'tune.down', steps: -2, pin: 'C' },
        ]);
        c.stop();
        await window.close();
    },

    async 'pairs what a dial turns'() {
        assert.deepStrictEqual(turnPair('volume.down'), { idUp: 'volume.up', idDown: 'volume.down' });
        assert.deepStrictEqual(turnPair('tune.up10'), { idUp: 'tune.up10', idDown: 'tune.down10' });
        assert.strictEqual(turnPair('tx.hold'), null);
        assert.strictEqual(validPin('E'), undefined);
        assert.strictEqual(validPin('a'), undefined);
        assert.strictEqual(validPin('D'), 'D');
    },

    // ---- the hosts' shells: what a host says reaches the link, and a gone host ends them ----

    async 'Elgato: host events reach the link, and the plugin exits with its host'() {
        const home = scratchHome();
        const window = new StubWindow(path.join(home, 'e.sock'));
        await window.listening;
        writeDiscovery(home, 'e', path.join(home, 'e.sock'));
        const host = new WebSocket.Server({ host: '127.0.0.1', port: 0 });
        await once(host, 'listening');
        const child = spawn(process.execPath, [path.join(kFolder, 'plugin.js'), '-port', String(host.address().port),
            '-pluginUUID', 'U', '-registerEvent', 'registerPlugin', '-info', '{}'],
        { env: { ...process.env, HOME: home }, stdio: 'ignore' });
        const exited = once(child, 'exit');
        const [socket] = await once(host, 'connection');
        const said = [];
        socket.on('message', (data) => said.push(JSON.parse(data.toString())));
        await until(() => said.length > 0, 'registration');
        assert.deepStrictEqual(said[0], { event: 'registerPlugin', uuid: 'U' });
        await window.heard('hello');
        assert.strictEqual(window.lines[0].plugin, 'elgato');

        const tell = (message) => socket.send(JSON.stringify(message));
        const action = (id) => `com.nereussdr.radio.${id}`;
        tell({ event: 'willAppear', action: action('tx.mox'), context: 'm', device: 'd', payload: { controller: 'Keypad' } });
        tell({ event: 'willAppear', action: action('tx.hold'), context: 'h', device: 'd', payload: { controller: 'Keypad' } });
        tell({ event: 'willAppear', action: action('tune.up'), context: 'v', device: 'd', payload: { controller: 'Encoder' } });
        tell({ event: 'keyDown', action: action('tx.hold'), context: 'h', device: 'd', payload: { settings: { slice: 'B' } } });
        tell({ event: 'keyUp', action: action('tx.hold'), context: 'h', device: 'd', payload: { settings: {} } });
        tell({ event: 'dialRotate', action: action('tune.up'), context: 'v', device: 'd', payload: { ticks: -4, settings: {} } });
        await window.heard('turn');
        assert.deepStrictEqual(window.lines.slice(1), [
            { type: 'press', id: 'tx.hold', pin: 'B' },
            { type: 'release', id: 'tx.hold' },
            { type: 'turn', idUp: 'tune.up', idDown: 'tune.down', steps: -4 },
        ]);

        // A key that vanishes while down still lets go.
        tell({ event: 'keyDown', action: action('tx.hold'), context: 'h', device: 'd', payload: { settings: {} } });
        tell({ event: 'willDisappear', action: action('tx.hold'), context: 'h', device: 'd', payload: {} });
        await window.heard('release', 2);

        // Lights and the dial's text.
        const before = said.length;
        window.broadcast('{"type":"state","id":"tx.mox","on":true}\n');
        window.broadcast('{"type":"display","frequency":"7.100.000","mode":"LSB","filter":"2.7K","slice":"C"}\n');
        await until(() => said.length >= before + 2, 'the paints');
        assert.deepStrictEqual(said[before], { event: 'setImage', context: 'm', payload: { target: 0, image: 'imgs/key-on.png' } });
        assert.deepStrictEqual(said[before + 1], { event: 'setFeedback', context: 'v', payload: { title: 'Slice C', value: '7.100.000' } });

        tell({ event: 'keyDown', action: action('tx.mox'), context: 'm', device: 'd', payload: { settings: {} } });
        await window.heard('press', 3);
        const socketsBefore = window.sockets.size;
        assert.strictEqual(socketsBefore, 1);
        socket.close();
        assert.strictEqual((await exited)[0], 0);
        await window.until(() => window.sockets.size === 0, 'the socket to drop');
        host.close();
        await window.close();
    },

    async 'Ulanzi: host events reach the link, and the plugin exits with its host'() {
        const home = scratchHome();
        const window = new StubWindow(path.join(home, 'u.sock'));
        await window.listening;
        writeDiscovery(home, 'u', path.join(home, 'u.sock'));
        const host = new WebSocket.Server({ host: '127.0.0.1', port: 0 });
        await once(host, 'listening');
        const child = spawn(process.execPath, [path.join(kUlanziFolder, 'plugin', 'app.js'), '127.0.0.1', String(host.address().port), 'en'],
            { env: { ...process.env, HOME: home }, stdio: 'ignore' });
        const exited = once(child, 'exit');
        const [socket] = await once(host, 'connection');
        await window.heard('hello');
        assert.strictEqual(window.lines[0].plugin, 'ulanzi');

        const uuid = (id) => `com.nereussdr.radio.controller.${id}`;
        const tell = (cmd, id, key, more = {}) => socket.send(JSON.stringify({ cmd, uuid: uuid(id), key, actionid: `a${key}`, ...more }));
        tell('add', 'tx.hold', '0', { param: { slice: 'D' } });
        tell('add', 'tune.up', '1', { param: {} });
        tell('keydown', 'tx.hold', '0');
        tell('keyup', 'tx.hold', '0');
        tell('dialrotate', 'tune.up', '1', { rotateEvent: 'left' });
        tell('dialrotate', 'tune.up', '1', { rotateEvent: 'right' });
        await window.heard('turn', 2);
        assert.deepStrictEqual(window.lines.slice(1), [
            { type: 'press', id: 'tx.hold', pin: 'D' },
            { type: 'release', id: 'tx.hold' },
            { type: 'turn', idUp: 'tune.up', idDown: 'tune.down', steps: -1 },
            { type: 'turn', idUp: 'tune.up', idDown: 'tune.down', steps: 1 },
        ]);

        // Taken off the deck while down: lets go. The SDK adds each one's context.
        tell('keydown', 'tx.hold', '0');
        socket.send(JSON.stringify({ cmd: 'clear', param: [{ uuid: uuid('tx.hold'), key: '0', actionid: 'a0' }] }));
        await window.heard('release', 2);

        tell('add', 'tx.hold', '0', { param: {} });
        tell('keydown', 'tx.hold', '0');
        await window.heard('press', 2);
        socket.close();
        assert.strictEqual((await exited)[0], 0);
        await window.until(() => window.sockets.size === 0, 'the socket to drop');
        host.close();
        await window.close();
    },
};

async function run() {
    let failed = 0;
    for (const [name, test] of Object.entries(tests)) {
        try {
            await test();
            console.log(`ok   ${name}`);
        } catch (error) {
            failed += 1;
            console.log(`FAIL ${name}\n${error.stack}`);
        }
    }
    console.log(`${Object.keys(tests).length - failed} passed, ${failed} failed`);
    process.exit(failed === 0 ? 0 : 1);
}

if (process.argv[2] === '--integration') {
    integration(process.argv[3]);
} else {
    run();
}
