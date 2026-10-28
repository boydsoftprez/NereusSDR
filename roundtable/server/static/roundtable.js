/* no-port-check: NereusSDR-original.
   Roundtable web pages, shared code and the public page.

   RT (window.RT) holds what every page uses: a DOM builder, the text
   cleaning and formatting of wire spec 6.10, S-units, frequency and time
   formatting, and fetch and form helpers. Every text goes into the page as
   a text node; the only markup a message can carry (bold, italic, http
   and https links) is built from DOM nodes here, never from HTML.

   The public page (body data-page="public") reads the live stream at
   /v1/live: the band list of every band, and a live view of the open room.
   The rows' readings, last message and talk time are not in a band-list
   summary, so the page fills them from the read API for the rows on
   screen, within a budget below the API's 60 requests a minute. */
"use strict";

(function () {
  const RT = {};
  window.RT = RT;

  // ------------------------------------------------------------ DOM

  /* el("div", {class: "x", text: "y", on: {click: f}, attrs: {...}}, kids)
     Children are nodes or strings; strings become text nodes. */
  function el(tag, props, kids) {
    const node = document.createElement(tag);
    if (props) {
      for (const key of Object.keys(props)) {
        const value = props[key];
        if (value === undefined || value === null || value === false) {
          continue;
        }
        if (key === "class") {
          node.className = value;
        } else if (key === "text") {
          node.textContent = String(value);
        } else if (key === "on") {
          for (const name of Object.keys(value)) {
            node.addEventListener(name, value[name]);
          }
        } else if (key === "data") {
          for (const name of Object.keys(value)) {
            node.dataset[name] = String(value[name]);
          }
        } else if (key === "attrs") {
          for (const name of Object.keys(value)) {
            if (value[name] !== null && value[name] !== undefined && value[name] !== false) {
              node.setAttribute(name, value[name] === true ? "" : String(value[name]));
            }
          }
        } else {
          node[key] = value;
        }
      }
    }
    add(node, kids);
    return node;
  }

  function add(node, kids) {
    if (kids === undefined || kids === null || kids === false) {
      return node;
    }
    if (!Array.isArray(kids)) {
      kids = [kids];
    }
    for (const kid of kids) {
      if (kid === undefined || kid === null || kid === false) {
        continue;
      }
      if (Array.isArray(kid)) {
        add(node, kid);
      } else if (typeof kid === "string" || typeof kid === "number") {
        node.appendChild(document.createTextNode(String(kid)));
      } else {
        node.appendChild(kid);
      }
    }
    return node;
  }

  function clear(node) {
    while (node.firstChild) {
      node.removeChild(node.firstChild);
    }
    return node;
  }

  function glyph(state) {
    return el("span", { class: "gl " + state, attrs: { "aria-hidden": "true" } });
  }

  RT.el = el;
  RT.add = add;
  RT.clear = clear;
  RT.glyph = glyph;

  // ------------------------------------------------------------ text (wire spec 6.10)

  const REMOVED = /[\u061C\u200B\u200E\u200F\u202A-\u202E\u2060\u2066-\u2069\uFEFF]/g;
  const CONTROLS = /[\u0000-\u0009\u000B-\u001F\u007F-\u009F]/g;
  const LINK = /https?:\/\/[^\s\p{Cf}]*/gu;
  const EMPHASIS = /\*\*([^*\n]+)\*\*|\*([^*\n]+)\*/g;

  /* Cleaning, in the wire spec's order: CR LF and lone CR become LF;
     every other control character goes; the invisible and
     direction-changing format characters go; NFC, after the removals;
     then the ends are trimmed. */
  function clean(text) {
    let out = String(text === null || text === undefined ? "" : text);
    out = out.replace(/\r\n?/g, "\n");
    out = out.replace(CONTROLS, "");
    out = out.replace(REMOVED, "");
    return out.normalize("NFC").trim();
  }

  /* The link runs a screen shows: from http:// or https:// to the next
     white space or format character. */
  function links(text) {
    const out = [];
    for (const m of String(text).matchAll(LINK)) {
      out.push(m[0]);
    }
    return out;
  }

  function emphasis(parent, text) {
    let last = 0;
    for (const m of text.matchAll(EMPHASIS)) {
      if (m.index > last) {
        parent.appendChild(document.createTextNode(text.slice(last, m.index)));
      }
      if (m[1] !== undefined) {
        parent.appendChild(el("b", { text: m[1] }));
      } else {
        parent.appendChild(el("i", { text: m[2] }));
      }
      last = m.index + m[0].length;
    }
    if (last < text.length) {
      parent.appendChild(document.createTextNode(text.slice(last)));
    }
  }

  /* A message's text as DOM: cleaned again, then links shown with their
     full address, then bold and italic. Nothing else is markup. */
  function formatted(text) {
    const frag = document.createDocumentFragment();
    const cleaned = clean(text);
    let last = 0;
    for (const m of cleaned.matchAll(LINK)) {
      if (m.index > last) {
        emphasis(frag, cleaned.slice(last, m.index));
      }
      const url = m[0];
      frag.appendChild(el("a", { href: url, text: url, attrs: { rel: "noopener noreferrer nofollow", target: "_blank" } }));
      last = m.index + url.length;
    }
    if (last < cleaned.length) {
      emphasis(frag, cleaned.slice(last));
    }
    return frag;
  }

  /* One line, plain: for list rows. */
  function plainLine(text) {
    return clean(text).replace(/\s+/g, " ");
  }

  RT.clean = clean;
  RT.links = links;
  RT.formatted = formatted;
  RT.plainLine = plainLine;

  // ------------------------------------------------------------ units and formats

  /* S-units on the app's scale: S9 is -73 dBm, 6 dB a unit, then S9 plus
     dB. */
  function sText(dbm) {
    if (dbm === null || dbm === undefined) {
      return "";
    }
    if (dbm > -73) {
      return "S9+" + Math.round(dbm + 73);
    }
    const s = Math.max(0, Math.round(9 + (dbm + 73) / 6));
    return "S" + s;
  }

  /* 0 to 1 along a bar that ends at S9+40. */
  function sBar(dbm) {
    if (dbm === null || dbm === undefined) {
      return 0;
    }
    const units = dbm > -73 ? 9 + (dbm + 73) / 10 : 9 + (dbm + 73) / 6;
    return Math.max(0, Math.min(1, units / 13));
  }

  function freqParts(hz) {
    const mhz = Math.floor(hz / 1e6);
    const khz = Math.floor(hz / 1e3) % 1000;
    const rest = hz % 1000;
    return [String(mhz), String(khz).padStart(3, "0"), String(rest).padStart(3, "0")];
  }

  function freqText(hz) {
    return freqParts(hz).join(".");
  }

  /* 14.212<i>.000</i> as DOM. */
  function freqNode(cls, hz) {
    const p = freqParts(hz);
    return el("span", { class: cls }, [p[0] + "." + p[1], el("i", { text: "." + p[2] })]);
  }

  function pad2(n) {
    return String(n).padStart(2, "0");
  }

  function hhmm(ms) {
    const d = new Date(ms);
    return pad2(d.getUTCHours()) + ":" + pad2(d.getUTCMinutes());
  }

  function dateText(ms) {
    const d = new Date(ms);
    return d.getUTCFullYear() + "-" + pad2(d.getUTCMonth() + 1) + "-" + pad2(d.getUTCDate());
  }

  function mmss(seconds) {
    const s = Math.max(0, Math.floor(seconds));
    return Math.floor(s / 60) + ":" + pad2(s % 60);
  }

  function ago(ms) {
    const s = Math.max(0, Math.floor(ms / 1000));
    if (s < 60) {
      return s + "s";
    }
    if (s < 3600) {
      return Math.floor(s / 60) + "m";
    }
    if (s < 86400) {
      return Math.floor(s / 3600) + "h";
    }
    return Math.floor(s / 86400) + "d";
  }

  function callText(callsign, nth) {
    return nth && nth > 1 ? callsign + " (" + nth + ")" : callsign;
  }

  const FAMILY_LABEL = { lsb: "LSB", usb: "USB", digl: "DIGL", digu: "DIGU", am: "AM", fm: "FM", cw: "CW", fdvl: "FreeDV", fdvu: "FreeDV" };

  // Wire spec 6.2.
  const BANDS = [
    ["2200m", 135700, 137800], ["630m", 472000, 479000], ["160m", 1800000, 2000000], ["80m", 3500000, 4000000],
    ["60m", 5250000, 5450000], ["40m", 7000000, 7300000], ["30m", 10100000, 10150000], ["20m", 14000000, 14350000],
    ["17m", 18068000, 18168000], ["15m", 21000000, 21450000], ["12m", 24890000, 24990000], ["10m", 28000000, 29700000],
    ["6m", 50000000, 54000000], ["4m", 70000000, 71000000], ["2m", 144000000, 148000000], ["1.25m", 222000000, 225000000],
    ["70cm", 420000000, 450000000], ["33cm", 902000000, 928000000], ["23cm", 1240000000, 1300000000],
  ];
  // The bands always shown, with or without rooms; others show when they have one.
  const ALWAYS = ["160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"];

  function bandShort(name) {
    if (name === "other") {
      return "Other";
    }
    return /^[0-9.]+m$/.test(name) ? name.slice(0, -1) : name;
  }

  function bandLong(name) {
    if (name === "other") {
      return "Other";
    }
    return /^[0-9.]+m$/.test(name) ? name.slice(0, -1) + " m" : name.replace(/cm$/, " cm");
  }

  RT.sText = sText;
  RT.sBar = sBar;
  RT.freqText = freqText;
  RT.freqParts = freqParts;
  RT.freqNode = freqNode;
  RT.hhmm = hhmm;
  RT.dateText = dateText;
  RT.mmss = mmss;
  RT.ago = ago;
  RT.callText = callText;
  RT.FAMILY_LABEL = FAMILY_LABEL;
  RT.BANDS = BANDS;
  RT.bandShort = bandShort;
  RT.bandLong = bandLong;

  // ------------------------------------------------------------ requests

  async function getJson(url) {
    const response = await fetch(url, { credentials: "same-origin", cache: "no-store", headers: { Accept: "application/json" } });
    let body = null;
    try {
      body = await response.json();
    } catch (e) {
      body = null;
    }
    return { status: response.status, body: body, headers: response.headers };
  }

  /* A form POST; the server checks the Origin the browser sends. A 303
     answer is followed by the browser and shows as `redirected`. */
  async function postForm(url, fields) {
    const body = new URLSearchParams();
    for (const key of Object.keys(fields)) {
      body.append(key, String(fields[key]));
    }
    const response = await fetch(url, {
      method: "POST",
      credentials: "same-origin",
      cache: "no-store",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: body.toString(),
    });
    let json = null;
    try {
      json = await response.clone().json();
    } catch (e) {
      json = null;
    }
    return { status: response.status, body: json, redirected: response.redirected, url: response.url };
  }

  function reasonOf(result, fallback) {
    if (result && result.body && typeof result.body.reason === "string") {
      return result.body.reason;
    }
    return fallback || "Roundtable can't answer right now.";
  }

  RT.getJson = getJson;
  RT.postForm = postForm;
  RT.reasonOf = reasonOf;

  /* The signed-in station and its CSRF token, from GET /v1/me. */
  async function me() {
    const r = await getJson("/v1/me");
    if (r.status === 200 && r.body) {
      return r.body;
    }
    return null;
  }

  RT.me = me;

  function toast(text) {
    const node = document.getElementById("toast");
    if (!node) {
      return;
    }
    node.textContent = text;
    node.classList.remove("hidden");
    clearTimeout(toast.timer);
    toast.timer = setTimeout(function () {
      node.classList.add("hidden");
    }, 3500);
  }

  RT.toast = toast;

  /* The top bar the signed-in pages share. */
  function topBar(title, station) {
    const who = station
      ? el("span", { class: "who" }, ["Signed in as ", el("b", { text: station.callsign || "" })])
      : null;
    return el("header", { class: "top" }, [
      el("a", { class: "brand", href: "/" }, [
        el("img", { src: "/nereus-64.png", alt: "" }),
        el("span", { class: "word" }, ["Nereus", el("b", { text: "SDR" })]),
        el("span", { class: "sep" }),
        el("span", { class: "rt", text: "Roundtable" }),
      ]),
      el("h1", { text: title }),
      el("span", { class: "fill" }),
      who,
    ]);
  }

  RT.topBar = topBar;

  function goneText(state) {
    if (state === "removed") {
      return "Message removed";
    }
    if (state === "deleted") {
      return "Deleted by sender";
    }
    return "Message removed after a year";
  }

  RT.goneText = goneText;

  // The public page replaces this with its search highlighting.
  RT.highlight = function (text) {
    return text;
  };

  function messageNode(m) {
    if (m.state !== "shown") {
      return el("div", { class: "msg gone" }, [
        el("div", { class: "h" }, [el("span", { class: "c", text: callText(m.callsign, m.nth) }), el("span", { class: "t", text: hhmm(m.t) })]),
        el("div", { class: "x", text: goneText(m.state) }),
      ]);
    }
    return el("div", { class: "msg", data: { msg: m.id } }, [
      el("div", { class: "h" }, [
        el("span", { class: "c" }, RT.highlight(callText(m.callsign, m.nth))),
        m.dbm !== null ? el("span", { class: "hr", text: "reading " + sText(m.dbm) }) : null,
        el("span", { class: "t", text: hhmm(m.t) }),
      ]),
      el("div", { class: "x" }, formatted(m.text)),
    ]);
  }

  RT.messageNode = messageNode;


  // ============================================================ public page

  if (!document.body || document.body.dataset.page !== "public") {
    return;
  }

  const OFFLINE_AFTER_MS = 20000;
  const READING_STALE_MS = 20000;
  const ROOM_STALE_MS = 60000;
  const BACKOFF_S = [1, 2, 5, 10, 30];
  const EARLIER_MS = 30 * 60000;
  // The read API takes 60 a minute per address group; the page keeps under 40.
  const BUCKET_SIZE = 20;
  const BUCKET_PER_MS = 40 / 60000;

  const S = { band: null, q: "", quiet: false, sort: "f", sel: null, phoneRoom: false };
  const rooms = new Map();
  const details = new Map();
  const searchRooms = new Map();
  let listGen = 0;
  let view = null;
  let viewGen = 0;
  let ws = null;
  let link = "connecting";
  let everLive = false;
  let lostAt = 0;
  let connectStarted = Date.now();
  let attempt = 0;
  let retryTimer = null;
  let lastHeard = 0;
  let offset = 0;
  let tokens = BUCKET_SIZE;
  let tokensAt = Date.now();
  let inflight = 0;
  let apiPausedUntil = 0;
  let pendingFromLink = null;

  const $ = function (id) {
    return document.getElementById(id);
  };
  const rowsBox = $("rows");
  const roomBox = $("room");
  const q = $("q");

  function now() {
    return Date.now() + offset;
  }

  function noteTime(serverMs) {
    if (typeof serverMs === "number") {
      offset = serverMs - Date.now();
    }
  }

  // ------------------------------------------------------------ room data

  function roomDetail(id) {
    if (view && view.id === id) {
      return { room: view, live: true, closed: null };
    }
    const d = details.get(id);
    return d ? d.data : null;
  }

  function openTurn(room) {
    if (!room || !room.now || !room.now.length) {
      return null;
    }
    return room.now[0];
  }

  function talkerRefs(turn) {
    const refs = new Set();
    if (turn) {
      for (const t of turn.talkers) {
        if (t.ref) {
          refs.add(t.ref);
        }
      }
    }
    return refs;
  }

  /* Readings of the open turn, nearest first; rows with no distance follow
     in callsign order (wire spec 6.8). */
  function readingsOf(room) {
    const turn = openTurn(room);
    if (!room || !turn) {
      return [];
    }
    const skip = talkerRefs(turn);
    const out = room.members.filter(function (m) {
      return !skip.has(m.ref) && m.reading && m.reading.turn === turn.id && !m.joining;
    });
    out.sort(function (a, b) {
      if (a.km !== null && b.km !== null) {
        return a.km - b.km;
      }
      if (a.km !== null) {
        return -1;
      }
      if (b.km !== null) {
        return 1;
      }
      const c = a.callsign.localeCompare(b.callsign);
      return c !== 0 ? c : a.nth - b.nth;
    });
    return out;
  }

  function newestReading(room) {
    let t = 0;
    if (room) {
      for (const m of room.members) {
        if (m.reading && m.reading.t > t) {
          t = m.reading.t;
        }
      }
    }
    return t;
  }

  /* Quiet since the newest ended turn: the room view's when it is at hand,
     otherwise the summary's `ended`, otherwise `last` (wire spec 6.11). */
  function quietSince(summary, room) {
    if (room && room.earlier && room.earlier.length && room.earlier[0].end) {
      return room.earlier[0].end;
    }
    return summary.ended !== null && summary.ended !== undefined ? summary.ended : summary.last;
  }

  function isQuiet(summary) {
    return summary.state === "quiet" || summary.folded;
  }

  function talkerLabel(summary) {
    const t = summary.talkers || [];
    if (summary.state === "unknown") {
      return "Unknown";
    }
    if (summary.state === "doubled") {
      if (t.length >= 2) {
        return t[0] + " + " + t[1];
      }
      if (t.length === 1) {
        return t[0] + " + ?";
      }
      return "Two at once";
    }
    return t.join(", ");
  }

  // ------------------------------------------------------------ search

  function freqQuery(text) {
    return /^[0-9]+(\.[0-9]*)*$/.test(text) && /[0-9]/.test(text);
  }

  /* A typed frequency in Hz: with a dot it is MHz (14.212 or 14.212.5);
     without, up to 3 digits is MHz, 4 to 6 kHz, 7 or more Hz. */
  function typedHz(text) {
    const parts = text.split(".");
    if (parts.length > 1) {
      const mhz = parseInt(parts[0] || "0", 10);
      const khz = parts[1] ? parseFloat("0." + parts[1]) * 1000 : 0;
      const rest = parts.slice(2).join("");
      const hz = rest ? parseFloat("0." + rest) * 1000 : 0;
      return Math.round(mhz * 1e6 + khz * 1e3 + hz);
    }
    const n = parseInt(text, 10);
    if (text.length <= 3) {
      return n * 1e6;
    }
    if (text.length <= 6) {
      return n * 1e3;
    }
    return n;
  }

  function freqMatches(summary, text) {
    const f = freqText(summary.hz);
    const digits = f.replace(/\./g, "");
    if (text.indexOf(".") >= 0) {
      return f.startsWith(text);
    }
    return digits.startsWith(text) || f.startsWith(text);
  }

  /* What a callsign search matched in a room: "talker", a reading row, a
     poster, or nothing. */
  function callMatch(summary, upper) {
    if ((summary.talkers || []).some(function (c) {
      return c.toUpperCase().includes(upper);
    })) {
      return { kind: "talker" };
    }
    const room = roomDetail(summary.id) && roomDetail(summary.id).room;
    if (room) {
      const reader = readingsOf(room).find(function (m) {
        return m.callsign.toUpperCase().includes(upper);
      });
      if (reader) {
        return { kind: "reader", member: reader };
      }
      const msgs = (room.messages || []).slice().reverse();
      const post = msgs.find(function (m) {
        return m.callsign.toUpperCase().includes(upper);
      });
      if (post) {
        return { kind: "poster", message: post };
      }
    }
    const lastLine = summary.lastMessage;
    if (lastLine && lastLine.callsign.toUpperCase().includes(upper)) {
      return { kind: "poster", message: lastLine };
    }
    if (room) {
      const member = room.members.find(function (m) {
        return m.callsign.toUpperCase().includes(upper);
      });
      if (member) {
        return { kind: "member", member: member };
      }
    }
    const found = searchRooms.get(upper);
    if (found && found.has(summary.id)) {
      return { kind: "member" };
    }
    return null;
  }

  function matches(summary) {
    if (!S.q) {
      return true;
    }
    if (freqQuery(S.q)) {
      return freqMatches(summary, S.q);
    }
    return callMatch(summary, S.q.toUpperCase()) !== null;
  }

  let searchTimer = null;

  function searchApi() {
    clearTimeout(searchTimer);
    const text = S.q.toUpperCase();
    if (!text || freqQuery(text) || !/^[A-Z0-9/]{3,16}$/.test(text) || !/[A-Z]/.test(text) || !/[0-9]/.test(text)) {
      return;
    }
    searchTimer = setTimeout(async function () {
      if (searchRooms.has(text) || !take()) {
        return;
      }
      const r = await api("/v1/api/callsign/" + encodeURIComponent(text));
      if (r && r.status === 200 && r.body) {
        const ids = new Set(r.body.rows.map(function (row) {
          return row.room;
        }));
        searchRooms.set(text, ids);
        for (const id of ids) {
          if (rooms.has(id) && !details.has(id)) {
            fetchDetail(id, true);
          }
        }
        renderList();
      }
    }, 400);
  }

  // ------------------------------------------------------------ read API

  function take() {
    const t = Date.now();
    tokens = Math.min(BUCKET_SIZE, tokens + (t - tokensAt) * BUCKET_PER_MS);
    tokensAt = t;
    if (t < apiPausedUntil || tokens < 1) {
      return false;
    }
    tokens -= 1;
    return true;
  }

  async function api(url) {
    inflight += 1;
    try {
      const r = await getJson(url);
      if (r.body && typeof r.body.time === "number") {
        noteTime(r.body.time);
      }
      if (r.status === 429) {
        const retry = parseInt(r.headers.get("Retry-After") || "5", 10);
        apiPausedUntil = Date.now() + Math.max(1, retry) * 1000;
      }
      return r;
    } catch (e) {
      return null;
    } finally {
      inflight -= 1;
    }
  }

  async function fetchDetail(id) {
    if (typeof id !== "string") {
      return;
    }
    const summary = rooms.get(id);
    const parts = id.split("-");
    if (parts.length !== 2) {
      return;
    }
    if (!take()) {
      return;
    }
    const entry = details.get(id) || { at: 0, data: null };
    entry.at = Date.now();
    details.set(id, entry);
    const r = await api("/v1/api/room/" + parts[0] + "/" + parts[1]);
    if (r && r.status === 200 && r.body) {
      entry.data = { room: r.body.room, live: r.body.live, closed: r.body.closed };
      entry.at = Date.now();
      scheduleRender();
    } else if (!summary) {
      details.delete(id);
    }
  }

  // ------------------------------------------------------------ live stream

  function send(obj) {
    if (ws && ws.readyState === 1) {
      ws.send(JSON.stringify(obj));
    }
  }

  function connect() {
    clearTimeout(retryTimer);
    const scheme = location.protocol === "https:" ? "wss:" : "ws:";
    let sock;
    try {
      sock = new WebSocket(scheme + "//" + location.host + "/v1/live");
    } catch (e) {
      return failed();
    }
    ws = sock;
    sock.addEventListener("open", function () {
      if (ws !== sock) {
        return;
      }
      attempt = 0;
      send({ type: "watchBand", band: "all" });
      if (S.sel) {
        send({ type: "watch", room: S.sel });
      }
    });
    sock.addEventListener("message", function (event) {
      if (ws !== sock) {
        return;
      }
      let msg;
      try {
        msg = JSON.parse(event.data);
      } catch (e) {
        return;
      }
      lastHeard = Date.now();
      onMessage(msg);
    });
    sock.addEventListener("close", function () {
      if (ws !== sock) {
        return;
      }
      ws = null;
      failed();
    });
  }

  function failed() {
    if (link === "live") {
      link = "reconnecting";
      lostAt = Date.now();
    }
    const delay = BACKOFF_S[Math.min(attempt, BACKOFF_S.length - 1)] * 1000;
    attempt += 1;
    retryTimer = setTimeout(connect, delay);
    renderLink();
  }

  RT.reconnectNow = function () {
    attempt = 0;
    if (!ws) {
      connect();
    }
  };

  function onMessage(msg) {
    if (msg.type === "rooms") {
      rooms.clear();
      for (const r of msg.rooms) {
        rooms.set(r.id, r);
      }
      listGen = msg.gen;
      if (!everLive) {
        noteServerFromRooms(msg.rooms);
      }
      link = "live";
      everLive = true;
      afterRooms();
    } else if (msg.type === "roomsDelta") {
      if (msg.gen !== listGen + 1) {
        send({ type: "resync", room: null });
        return;
      }
      listGen = msg.gen;
      for (const r of msg.add) {
        rooms.set(r.id, r);
      }
      for (const r of msg.change) {
        rooms.set(r.id, r);
      }
      for (const id of msg.remove) {
        rooms.delete(id);
      }
      scheduleRender();
    } else if (msg.type === "room") {
      if (msg.id !== S.sel) {
        send({ type: "unwatch", room: msg.id });
        return;
      }
      view = {
        id: msg.id, opening: msg.opening, band: msg.band, family: msg.family, hz: msg.hz,
        members: msg.members, now: msg.now, earlier: msg.earlier, messages: msg.messages,
      };
      viewGen = msg.gen;
      details.set(msg.id, { at: Date.now(), data: { room: view, live: true, closed: null } });
      scheduleRender();
    } else if (msg.type === "delta") {
      if (!view || msg.room !== view.id) {
        return;
      }
      if (msg.gen !== viewGen + 1) {
        send({ type: "resync", room: view.id });
        return;
      }
      viewGen = msg.gen;
      applyDelta(view, msg);
      scheduleRender();
    } else if (msg.type === "error") {
      if (msg.about === "watch" && S.sel) {
        // Not live: the read API answers its last opening.
        fetchDetail(S.sel, true);
      }
    }
  }

  function noteServerFromRooms(list) {
    // Until the read API gives the server's time, the newest summary bounds it.
    let newest = 0;
    for (const r of list) {
      newest = Math.max(newest, r.last);
    }
    if (newest > Date.now() + offset) {
      offset = newest - Date.now();
    }
  }

  function replaceBy(list, item, key) {
    const i = list.findIndex(function (x) {
      return x[key] === item[key];
    });
    if (i >= 0) {
      list[i] = item;
    } else {
      list.push(item);
    }
  }

  function applyDelta(room, d) {
    for (const m of d.members) {
      replaceBy(room.members, m, "ref");
    }
    if (d.left.length) {
      const gone = new Set(d.left);
      room.members = room.members.filter(function (m) {
        return !gone.has(m.ref);
      });
    }
    for (const t of d.turns) {
      room.now = room.now.filter(function (x) {
        return x.id !== t.id;
      });
      room.earlier = room.earlier.filter(function (x) {
        return x.id !== t.id;
      });
      if (t.end === null) {
        room.now.push(t);
      } else {
        room.earlier.unshift(t);
      }
    }
    const cut = now() - EARLIER_MS;
    room.earlier = room.earlier.filter(function (t) {
      return t.end === null || t.end >= cut;
    });
    room.earlier.sort(function (a, b) {
      return b.start - a.start;
    });
    for (const m of d.messages) {
      replaceBy(room.messages, m, "id");
    }
    room.messages.sort(function (a, b) {
      return a.id - b.id;
    });
    if (room.messages.length > 50) {
      room.messages = room.messages.slice(-50);
    }
    if (d.closed) {
      view = null;
      fetchDetail(room.id, true);
    }
  }

  function afterRooms() {
    if (pendingFromLink) {
      const id = pendingFromLink;
      pendingFromLink = null;
      const summary = rooms.get(id);
      if (summary) {
        S.band = summary.band;
      }
      select(id, false);
    } else if (!S.sel || !rooms.has(S.sel)) {
      const first = visibleOrder()[0];
      if (first) {
        select(first.id, false);
      }
    }
    renderAll();
  }

  // ------------------------------------------------------------ selection

  function select(id, scroll) {
    if (S.sel === id) {
      if (scroll) {
        scrollToSel();
      }
      return;
    }
    if (S.sel && view) {
      send({ type: "unwatch", room: S.sel });
    }
    S.sel = id;
    view = null;
    viewGen = 0;
    if (id && ws && ws.readyState === 1 && rooms.has(id)) {
      send({ type: "watch", room: id });
    } else if (id && (!details.has(id) || !details.get(id).data)) {
      fetchDetail(id, true);
    }
    renderAll();
    if (scroll) {
      scrollToSel();
    }
  }

  function scrollToSel() {
    const sel = rowsBox.querySelector(".row.sel");
    if (sel) {
      const box = rowsBox.getBoundingClientRect();
      const r = sel.getBoundingClientRect();
      if (r.top < box.top + 28 || r.bottom > box.bottom) {
        rowsBox.scrollTop += r.top - box.top - rowsBox.clientHeight / 2;
      }
    }
  }

  // ------------------------------------------------------------ list

  function bandOrder() {
    const present = new Set();
    for (const r of rooms.values()) {
      present.add(r.band);
    }
    const out = [];
    for (const b of BANDS) {
      if (ALWAYS.indexOf(b[0]) >= 0 || present.has(b[0])) {
        out.push(b[0]);
      }
    }
    if (present.has("other")) {
      out.push("other");
    }
    return out;
  }

  function sorted(list) {
    const out = list.slice();
    if (S.sort === "n") {
      out.sort(function (a, b) {
        return b.members - a.members || a.hz - b.hz;
      });
    } else if (S.sort === "t") {
      out.sort(function (a, b) {
        return b.last - a.last || a.hz - b.hz;
      });
    } else {
      out.sort(function (a, b) {
        return a.hz - b.hz;
      });
    }
    return out;
  }

  /* The rooms in list order, grouped by band, quiet ones folded unless
     shown, searched or selected. */
  function groups() {
    const out = [];
    const all = Array.from(rooms.values());
    for (const band of bandOrder()) {
      if (S.band && band !== S.band) {
        continue;
      }
      const inBand = all.filter(function (r) {
        return r.band === band && matches(r);
      });
      if (!inBand.length) {
        continue;
      }
      const quiet = inBand.filter(isQuiet);
      const shown = sorted(inBand).filter(function (r) {
        return S.quiet || S.q || !isQuiet(r) || r.id === S.sel;
      });
      out.push({ band: band, rooms: inBand, quiet: quiet.length, shown: shown });
    }
    return out;
  }

  function visibleOrder() {
    const out = [];
    for (const g of groups()) {
      for (const r of g.shown) {
        out.push(r);
      }
    }
    return out;
  }

  function highlight(text) {
    if (!S.q || freqQuery(S.q)) {
      return text;
    }
    const upper = text.toUpperCase();
    const needle = S.q.toUpperCase();
    const out = [];
    let i = 0;
    for (;;) {
      const j = upper.indexOf(needle, i);
      if (j < 0) {
        break;
      }
      if (j > i) {
        out.push(text.slice(i, j));
      }
      out.push(el("mark", { text: text.slice(j, j + needle.length) }));
      i = j + needle.length;
    }
    out.push(text.slice(i));
    return out;
  }

  RT.highlight = highlight;

  /* The row's strip: the open turn's readings from the band list, nearest
     first, at most 12 (wire spec 6.11). */
  function stripNode(summary) {
    const reads = summary.readings || [];
    if (!reads.length) {
      return el("span", { class: "strip none", attrs: { "aria-label": "no readings" } });
    }
    const strip = el("span", { class: "strip", attrs: { "aria-label": reads.length + " readings, nearest first" } });
    for (const dbm of reads) {
      const bar = el("i");
      bar.style.height = Math.round(3 + sBar(dbm) * 13) + "px";
      strip.appendChild(bar);
    }
    return strip;
  }

  /* A row is drawn from its band-list summary alone (wire spec 6.11). */
  function rowNode(summary) {
    const t = now();
    const quiet = isQuiet(summary);
    // `last` is the room's latest reading, row change, turn or message (wire spec 6.11).
    const stale = t - summary.last > ROOM_STALE_MS;
    const cls = ["row"];
    if (quiet) {
      cls.push("q");
    }
    if (stale) {
      cls.push("stale");
    }
    if (summary.id === S.sel) {
      cls.push("sel");
    }
    const state = quiet ? "quiet" : summary.state;
    let who;
    if (quiet) {
      const mins = Math.max(1, Math.floor((t - quietSince(summary, null)) / 60000));
      who = el("span", { class: "who" }, [glyph("quiet"), el("span", { class: "c quiet", text: (summary.folded ? "away " : "quiet ") + mins + " min" })]);
    } else {
      who = el("span", { class: "who" }, [glyph(state), el("span", { class: "c " + (state === "talking" ? "" : state) }, highlight(talkerLabel(summary)))]);
    }
    const open = summary.start !== null && summary.start !== undefined;
    const tm = el("span", { class: "tm r", text: open ? mmss((t - summary.start) / 1000) : "" });
    // An open turn's heard count; a quiet room's members.
    const heard = open ? summary.heard : summary.members;
    const hb = el("span", { class: "hb r", text: heard ? String(heard) : "" });
    const last = summary.lastMessage;
    const lm = el("span", { class: "lm" });
    const match = S.q && !freqQuery(S.q) ? callMatch(summary, S.q.toUpperCase()) : null;
    if (match && match.kind === "reader") {
      add(lm, el("span", { class: "via" }, ["heard by ", highlight(callText(match.member.callsign, match.member.nth)), " · " + sText(match.member.reading.dbm)]));
    } else if (match && match.kind === "poster") {
      add(lm, [el("b", {}, highlight(callText(match.message.callsign, match.message.nth))), messageLine(match.message)]);
    } else if (last) {
      add(lm, [el("b", {}, highlight(callText(last.callsign, last.nth))), messageLine(last)]);
    } else {
      add(lm, el("span", { class: "gone", text: "No messages yet" }));
    }
    const shownMsg = match && match.kind === "poster" ? match.message : last;
    const ageNode = el("span", { class: "ago r", text: shownMsg ? ago(t - shownMsg.t) : "" });
    const row = el(
      "div",
      {
        class: cls.join(" "),
        data: { id: summary.id },
        attrs: { role: "option", "aria-selected": summary.id === S.sel ? "true" : "false" },
        on: { click: function () {
          select(summary.id, false);
          if (window.matchMedia("(max-width: 760px)").matches) {
            enterPhoneRoom();
          }
        } },
      },
      [
        freqNode("fq", summary.hz),
        el("span", { class: "md" + (summary.family.indexOf("fdv") === 0 ? " fdv" : ""), text: FAMILY_LABEL[summary.family] || summary.family.toUpperCase() }),
        who, tm, hb, stripNode(summary), lm, ageNode,
      ]
    );
    return row;
  }

  function messageLine(m) {
    if (m.state === "shown") {
      return document.createTextNode(plainLine(m.text));
    }
    return el("span", { class: "gone", text: goneText(m.state) });
  }


  function renderList() {
    const keep = rowsBox.scrollTop;
    clear(rowsBox);
    const gs = groups();
    let shown = 0;
    let total = 0;
    for (const g of gs) {
      const talking = g.rooms.filter(function (r) {
        return r.state === "talking" && !r.folded;
      }).length;
      const head = el("div", { class: "grp" }, [
        el("span", { class: "bn", text: bandLong(g.band) }),
        el("span", { class: "gc", text: g.rooms.length + " rooms · " + talking + " talking" }),
      ]);
      if (g.quiet && !S.q) {
        head.appendChild(el("span", { class: "gq" }, [
          S.quiet ? g.quiet + " quiet shown ·" : g.quiet + " quiet folded ·",
          el("button", { class: "linkbtn", type: "button", text: S.quiet ? "Fold them" : "Show", on: { click: toggleQuiet } }),
        ]));
      }
      const section = el("section", { data: { band: g.band } }, head);
      for (const r of g.shown) {
        section.appendChild(rowNode(r));
      }
      rowsBox.appendChild(section);
      shown += g.shown.length;
      total += g.rooms.length;
    }
    if (!gs.length) {
      let text;
      if (!everLive) {
        text = "Connecting to Roundtable…";
      } else if (S.q) {
        text = "No room has " + S.q + " in it right now.";
      } else if (S.band) {
        text = "No rooms on " + bandLong(S.band) + " right now.";
      } else {
        text = "No rooms on the air right now.";
      }
      rowsBox.appendChild(el("div", { class: "empty-list", text: text }));
    }
    rowsBox.scrollTop = keep;
    $("shown").textContent = shown + " of " + total + " rooms shown";
  }

  function toggleQuiet() {
    S.quiet = !S.quiet;
    renderAll();
  }

  // ------------------------------------------------------------ bands and band map

  function renderBands() {
    const box = $("band-buttons");
    clear(box);
    const all = Array.from(rooms.values());
    function button(name, label, list) {
      const talking = list.filter(function (r) {
        return r.state === "talking" && !r.folded;
      }).length;
      const unknown = list.filter(function (r) {
        return (r.state === "unknown" || r.state === "doubled") && !r.folded;
      }).length;
      const bar = el("div", { class: "act" });
      if (list.length) {
        const i = el("i");
        i.style.width = (talking / list.length) * 100 + "%";
        const u = el("u");
        u.style.width = (unknown / list.length) * 100 + "%";
        add(bar, [i, u]);
      }
      const on = name === null ? S.band === null : S.band === name;
      return el(
        "button",
        {
          class: "band" + (name === null ? " all" : "") + (on ? " on" : ""),
          type: "button",
          disabled: name !== null && !list.length,
          data: { band: name === null ? "" : name },
          attrs: {
            "aria-pressed": on ? "true" : "false",
            "aria-label": (name === null ? "All bands" : bandLong(name)) + ", " + list.length + " rooms, " + talking + " talking, " + unknown + " unknown",
          },
          on: { click: function () {
            S.band = name;
            renderAll();
          } },
        },
        [el("div", { class: "l" }, [el("span", { class: "n", text: label }), el("span", { class: "c", text: String(list.length) })]), bar]
      );
    }
    box.appendChild(button(null, "All", all));
    for (const band of bandOrder()) {
      box.appendChild(button(band, bandShort(band), all.filter(function (r) {
        return r.band === band;
      })));
    }
    const quiet = all.filter(isQuiet).length;
    $("quiet-count").textContent = String(quiet);
    $("quiet-chip").classList.toggle("on", S.quiet);
    $("quiet-chip").setAttribute("aria-pressed", S.quiet ? "true" : "false");
  }

  function renderMap() {
    const box = $("bmap");
    clear(box);
    const band = BANDS.find(function (b) {
      return b[0] === S.band;
    });
    if (!band) {
      box.classList.add("hidden");
      return;
    }
    box.classList.remove("hidden");
    const lo = band[1];
    const hi = band[2];
    const inner = el("div", { class: "bin" });
    const axis = el("div", { class: "axis" });
    for (let i = 0; i <= 6; i += 1) {
      const tick = el("span", { text: ((lo + ((hi - lo) * i) / 6) / 1e6).toFixed(3) });
      tick.style.left = (i / 6) * 100 + "%";
      axis.appendChild(tick);
    }
    add(inner, [el("div", { class: "base" }), axis]);
    for (const r of rooms.values()) {
      if (r.band !== S.band) {
        continue;
      }
      if (isQuiet(r) && !S.quiet && r.id !== S.sel) {
        continue;
      }
      const state = isQuiet(r) ? "quiet" : r.state;
      const stem = el("span", { class: "stem" });
      stem.style.height = Math.min(52, 6 + r.members * 2.2) + "px";
      const mk = el(
        "button",
        {
          class: "mk" + (r.id === S.sel ? " sel" : ""),
          type: "button",
          data: { id: r.id, members: r.members },
          title: freqText(r.hz) + " " + (talkerLabel(r) || "quiet") + ", " + r.members + " stations",
          attrs: { "aria-label": freqText(r.hz) + ", " + r.members + " stations" },
          on: { click: function () {
            select(r.id, true);
          } },
        },
        [glyph(state), stem]
      );
      mk.style.left = ((r.hz - lo) / (hi - lo)) * 100 + "%";
      inner.appendChild(mk);
    }
    add(box, [el("span", { class: "bname", text: bandLong(S.band) }), inner]);
  }

  // ------------------------------------------------------------ connection line

  function linkState() {
    const t = Date.now();
    if (link === "connecting" && t - connectStarted > OFFLINE_AFTER_MS) {
      link = "offline";
      lostAt = connectStarted;
    }
    if (link === "reconnecting" && t - lostAt > OFFLINE_AFTER_MS) {
      link = "offline";
    }
    return link;
  }

  function renderLink() {
    const state = linkState();
    const box = $("live");
    box.className = "live " + state;
    const bands = $("bands");
    bands.classList.toggle("offline", state === "offline");
    bands.classList.toggle("connecting", state === "connecting");
    const words = { live: "LIVE", reconnecting: "RECONNECTING", offline: "OFFLINE", connecting: "CONNECTING" };
    $("live-state").textContent = words[state];
    let k = "";
    const list = Array.from(rooms.values());
    if (state === "live") {
      const onAir = list.filter(function (r) {
        return !isQuiet(r);
      }).length;
      const reading = list.reduce(function (a, r) {
        return a + r.members;
      }, 0);
      k = onAir + " rooms on the air · " + reading.toLocaleString("en-US") + " stations";
    } else if (state === "reconnecting") {
      k = "nothing new for " + Math.floor((Date.now() - lastHeard) / 1000) + " s";
    } else if (state === "offline") {
      k = "since " + hhmm(lostAt) + " UTC";
    }
    $("live-k").textContent = k;
    const conn = document.getElementById("conn");
    if (conn) {
      conn.textContent = connText();
      conn.className = "conn " + state;
    }
  }

  /* The room's connection line (design 11.1). */
  function connText() {
    const state = linkState();
    if (state === "reconnecting") {
      return "Reconnecting · nothing new for " + Math.floor((Date.now() - lastHeard) / 1000) + " s";
    }
    if (state === "offline") {
      return "Offline since " + hhmm(lostAt) + " UTC";
    }
    if (state === "connecting") {
      return "Connecting";
    }
    const detail = S.sel ? roomDetail(S.sel) : null;
    const newest = detail ? newestReading(detail.room) : 0;
    let line = "Live";
    if (newest) {
      line += " · last reading " + Math.max(0, Math.floor((now() - newest) / 1000)) + " s ago";
    }
    if (detail && !(view && view.id === S.sel)) {
      const d = details.get(S.sel);
      if (d && d.at) {
        line += " · updated " + Math.floor((Date.now() - d.at) / 1000) + " s ago";
      }
    }
    return line;
  }

  // ------------------------------------------------------------ room pane

  function roomLink(summaryLike) {
    return "https://" + location.host + "/r/" + summaryLike.family + "/" + summaryLike.hz;
  }

  RT.roomLink = roomLink;

  async function copyLink(summaryLike) {
    const url = roomLink(summaryLike);
    let ok = false;
    try {
      if (navigator.clipboard && navigator.clipboard.writeText) {
        await navigator.clipboard.writeText(url);
        ok = true;
      }
    } catch (e) {
      ok = false;
    }
    window.__rtCopied = url;
    toast(ok ? "Copied " + url : url);
  }

  function readingRow(m, t) {
    const age = t - m.reading.t;
    const stale = age > READING_STALE_MS;
    const bar = el("i");
    bar.style.width = sBar(m.reading.dbm) * 100 + "%";
    return el("div", { class: "rd" + (m.away ? " aw" : "") + (stale ? " stale" : ""), data: { ref: m.ref } }, [
      el("span", { class: "c" }, [highlight(callText(m.callsign, m.nth)), m.away ? el("span", { class: "away", text: "AWAY" }) : null]),
      el("span", { class: "k", text: m.km !== null ? m.km.toLocaleString("en-US") + " km" : "" }),
      el("span", { class: "o", text: m.offsetHz ? (m.offsetHz > 0 ? "+" : "") + m.offsetHz + " Hz" : "on" }),
      el("span", { class: "b" }, bar),
      el("span", { class: "v", text: sText(m.reading.dbm) }),
      el("span", { class: "age", text: Math.floor(Math.max(0, age) / 1000) + " s" }),
    ]);
  }

  function turnRow(turn) {
    const names = turn.talkers.map(function (x) {
      return callText(x.callsign, x.nth);
    });
    let who;
    let note;
    if (turn.kind === "unknown") {
      who = "Unknown";
      note = turn.names.length ? "named " + turn.names[0].name + " by " + turn.names[0].count : "not named";
      if (turn.names.length > 1) {
        note += ", " + turn.names.slice(1).map(function (n) {
          return n.name + " by " + n.count;
        }).join(", ");
      }
    } else if (turn.kind === "doubled") {
      who = names.length ? names.join(" + ") + (names.length < 2 ? " + ?" : "") : "Two at once";
      note = "two at once";
    } else {
      who = names.join(", ");
      note = turn.heard + " heard";
    }
    const length = ((turn.end || now()) - turn.start) / 1000;
    return el("div", { class: "turn", attrs: { role: "listitem", tabindex: "0" }, data: { turn: turn.id } }, [
      el("span", { class: "t", text: hhmm(turn.start) }),
      glyph(turn.kind),
      el("span", {}, [el("span", { class: "c " + (turn.kind === "talking" ? "" : turn.kind) }, highlight(who)), el("span", { class: "x", text: note })]),
      el("span", { class: "d", text: mmss(length) }),
    ]);
  }


  function renderRoom() {
    const focused = document.activeElement;
    const focusTurn = focused && focused.classList && focused.classList.contains("turn") ? focused.dataset.turn : null;
    const bodyFocused = focused && focused.classList && focused.classList.contains("rbody");
    const oldBody = roomBox.querySelector(".rbody");
    const keep = oldBody ? oldBody.scrollTop : 0;
    clear(roomBox);
    const summary = S.sel ? rooms.get(S.sel) : null;
    const detail = S.sel ? roomDetail(S.sel) : null;
    const room = detail ? detail.room : null;
    roomBox.appendChild(el("button", { class: "ph-back", type: "button", text: "‹ All rooms", on: { click: leavePhoneRoom } }));
    if (!S.sel || (!summary && !room)) {
      roomBox.appendChild(el("div", { class: "room-empty", text: everLive ? "Pick a room to read it." : "Connecting to Roundtable…" }));
      return;
    }
    const where = summary || room;
    const t = now();
    const turn = openTurn(room);
    const state = summary ? (isQuiet(summary) ? "quiet" : summary.state) : turn ? turn.kind : "quiet";
    const head = el("div", { class: "rhead" }, [
      el("div", { class: "l1" }, [
        freqNode("f", where.hz),
        el("span", { class: "tag", text: FAMILY_LABEL[where.family] || where.family }),
        el("span", { class: "tag", text: bandLong(where.band) }),
        el("button", { class: "btn small lnk", type: "button", id: "copy-link", text: "Copy link", title: roomLink(where), on: { click: function () {
          copyLink(where);
        } } }),
      ]),
      el("div", { class: "conn " + linkState(), id: "conn", text: connText() }),
    ]);
    if (state === "quiet") {
      const mins = Math.max(1, Math.floor((t - quietSince(summary || { last: detail && detail.closed ? detail.closed : t }, room)) / 60000));
      const line = detail && detail.live === false ? "This room closed at " + hhmm(detail.closed) + " UTC." : "Nobody has keyed for " + mins + " min. Readings are only kept while someone is keyed.";
      head.appendChild(el("div", { class: "talker" }, [glyph("quiet"), el("div", {}, [el("div", { class: "c quiet", text: "Quiet" }), el("div", { class: "sub", text: line })])]));
    } else {
      const label = summary ? talkerLabel(summary) : "";
      let line;
      if (state === "talking") {
        line = "talking " + (turn ? mmss((t - turn.start) / 1000) : "") + (turn ? " · " + turn.heard + " hear" : "");
      } else if (state === "unknown") {
        line = "keyed " + (turn ? mmss((t - turn.start) / 1000) : "") + " · not on Roundtable";
      } else {
        line = "two stations keyed at once" + (turn ? " for " + Math.floor((t - turn.start) / 1000) + " s" : "");
      }
      head.appendChild(el("div", { class: "talker" }, [
        glyph(state),
        el("div", {}, [el("div", { class: "c " + (state === "talking" ? "" : state), text: state === "unknown" ? "Unknown talker" : label }), el("div", { class: "sub", text: line })]),
      ]));
      if (turn && turn.kind === "unknown" && turn.names.length) {
        head.appendChild(el("div", { class: "names" }, ["Listeners have named this turn:"].concat(turn.names.map(function (n) {
          return el("span", { class: "nm" }, [n.name, el("span", { text: String(n.count) })]);
        }))));
      }
    }
    roomBox.appendChild(head);
    const body = el("div", { class: "rbody", attrs: { tabindex: "-1", "aria-label": "Room" } });
    if (!room) {
      body.appendChild(el("div", { class: "none", text: "Loading…" }));
    } else {
      if (turn && turn.kind !== "doubled") {
        const reads = readingsOf(room);
        const whom = turn.kind === "unknown" ? "THIS TALKER" : (summary ? talkerLabel(summary) : "").toUpperCase();
        body.appendChild(el("div", { class: "cap" }, ["HOW EACH STATION HEARS " + whom, el("span", { text: "nearest first" })]));
        if (reads.length) {
          for (const m of reads) {
            body.appendChild(readingRow(m, t));
          }
        } else {
          body.appendChild(el("div", { class: "none", text: "No readings of this turn yet." }));
        }
        body.appendChild(el("div", { class: "fn" }, [
          el("b", { text: "Every number is one station's own raw S-meter reading." }),
          " Radios read differently, so they are never averaged or ranked. AWAY is a station's radio reporting with nobody at it. Two unknown stations keyed at once show as one unknown turn.",
        ]));
      } else if (turn && turn.kind === "doubled") {
        body.appendChild(el("div", { class: "fn", text: "Two stations are keyed at once. Readings of a double are not shown as either talker's." }));
      }
      body.appendChild(el("div", { class: "cap" }, ["EARLIER TURNS", el("span", { text: "last 30 minutes" })]));
      const list = el("div", { attrs: { role: "list", "aria-label": "Earlier turns" } });
      for (const tn of room.earlier) {
        list.appendChild(turnRow(tn));
      }
      if (!room.earlier.length) {
        list.appendChild(el("div", { class: "none", text: "No earlier turns in the last 30 minutes." }));
      }
      body.appendChild(list);
      body.appendChild(el("div", { class: "cap" }, ["MESSAGES", el("span", { text: room.messages.length ? "newest last" : "" })]));
      if (room.messages.length) {
        for (const m of room.messages) {
          body.appendChild(messageNode(m));
        }
      } else {
        body.appendChild(el("div", { class: "none", text: "No messages yet." }));
      }
    }
    roomBox.appendChild(body);
    const p = freqParts(where.hz);
    roomBox.appendChild(el("div", { class: "ro" }, [
      el("span", {}, ["Read only. To post or name a turn, tune ", el("b", { text: p[0] + "." + p[1] + "." + p[2].slice(0, 2) }), " in NereusSDR or other Roundtable software."]),
      el("a", { class: "btn glass", href: "https://nereussdr.com/", text: "Get NereusSDR" }),
    ]));
    body.scrollTop = keep;
    if (focusTurn) {
      const again = body.querySelector('.turn[data-turn="' + focusTurn + '"]');
      if (again) {
        again.focus({ preventScroll: true });
      }
    } else if (bodyFocused) {
      body.focus({ preventScroll: true });
    }
  }

  function enterPhoneRoom() {
    document.body.classList.add("in-room");
    S.phoneRoom = true;
  }

  function leavePhoneRoom() {
    document.body.classList.remove("in-room");
    S.phoneRoom = false;
    rowsBox.focus({ preventScroll: true });
  }

  // ------------------------------------------------------------ rendering

  let renderQueued = false;

  function scheduleRender() {
    if (renderQueued) {
      return;
    }
    renderQueued = true;
    requestAnimationFrame(function () {
      renderQueued = false;
      renderAll();
    });
  }

  function renderAll() {
    renderBands();
    renderMap();
    renderList();
    renderRoom();
    renderLink();
    syncUrl();
  }

  RT.render = renderAll;

  function syncUrl() {
    if (!S.sel) {
      return;
    }
    const parts = S.sel.split("-");
    const path = "/r/" + parts[0] + "/" + parts[1];
    if (location.pathname !== path && location.pathname.indexOf("/r/") === 0) {
      history.replaceState(null, "", path);
    }
  }

  // ------------------------------------------------------------ input

  q.addEventListener("input", function () {
    S.q = q.value.trim();
    if (S.q && freqQuery(S.q)) {
      jumpToFrequency(S.q);
    }
    searchApi();
    renderAll();
  });

  /* A typed frequency selects the nearest matching room. */
  function jumpToFrequency(text) {
    const hz = typedHz(text);
    let best = null;
    for (const r of rooms.values()) {
      if (S.band && r.band !== S.band) {
        continue;
      }
      if (!freqMatches(r, text)) {
        continue;
      }
      if (!best || Math.abs(r.hz - hz) < Math.abs(best.hz - hz)) {
        best = r;
      }
    }
    if (best) {
      select(best.id, true);
    }
  }

  $("quiet-chip").addEventListener("click", toggleQuiet);
  $("sort").addEventListener("change", function (e) {
    S.sort = e.target.value;
    renderAll();
  });

  function focusRows() {
    rowsBox.focus({ preventScroll: true });
  }

  function moveSel(d) {
    const order = visibleOrder();
    if (!order.length) {
      return;
    }
    let i = order.findIndex(function (r) {
      return r.id === S.sel;
    });
    i = i < 0 ? 0 : Math.max(0, Math.min(order.length - 1, i + d));
    select(order[i].id, true);
  }

  /* Keyboard (U13): / searches; the arrows move through the rooms and each
     shows as you land on it; Enter goes into the room, where the arrows
     walk its earlier turns; Escape steps back: room, then search, then
     band. */
  document.addEventListener("keydown", function (e) {
    const a = document.activeElement;
    const inQ = a === q;
    const inRoom = a && a.closest && a.closest("#room");
    if (e.key === "/" && !inQ) {
      e.preventDefault();
      q.focus();
      q.select();
      return;
    }
    if ((e.key === "ArrowDown" || e.key === "ArrowUp") && !inRoom) {
      e.preventDefault();
      moveSel(e.key === "ArrowDown" ? 1 : -1);
      if (inQ) {
        q.focus();
      } else {
        focusRows();
      }
      return;
    }
    if ((e.key === "ArrowDown" || e.key === "ArrowUp") && inRoom) {
      const turns = Array.from(roomBox.querySelectorAll(".turn"));
      if (!turns.length) {
        return;
      }
      e.preventDefault();
      let i = turns.indexOf(a);
      if (i < 0) {
        i = e.key === "ArrowDown" ? 0 : turns.length - 1;
      } else {
        i = Math.max(0, Math.min(turns.length - 1, i + (e.key === "ArrowDown" ? 1 : -1)));
      }
      turns[i].focus();
      return;
    }
    if (e.key === "Enter" && !inRoom && (inQ || a === rowsBox || a === document.body)) {
      e.preventDefault();
      if (!S.sel) {
        moveSel(0);
      }
      const first = roomBox.querySelector(".turn");
      if (first) {
        first.focus();
      } else {
        const body = roomBox.querySelector(".rbody");
        if (body) {
          body.focus();
        }
      }
      if (window.matchMedia("(max-width: 760px)").matches) {
        enterPhoneRoom();
      }
      return;
    }
    if (e.key === "Escape") {
      e.preventDefault();
      if (inRoom || S.phoneRoom) {
        leavePhoneRoom();
        focusRows();
        return;
      }
      if (S.q) {
        S.q = "";
        q.value = "";
        renderAll();
        focusRows();
        return;
      }
      if (S.band) {
        S.band = null;
        renderAll();
        focusRows();
        return;
      }
      if (inQ) {
        focusRows();
      }
    }
  });

  // ------------------------------------------------------------ start

  (function fromLink() {
    const m = location.pathname.match(/^\/r\/([a-z]+)\/([1-9][0-9]{0,10})$/);
    if (m) {
      pendingFromLink = m[1] + "-" + m[2];
      S.sel = null;
    }
  })();

  // A room link to a room that is not live: show its last opening from the read API.
  if (pendingFromLink) {
    const id = pendingFromLink;
    setTimeout(function () {
      if (!rooms.has(id) && pendingFromLink === id) {
        pendingFromLink = null;
        S.sel = id;
        fetchDetail(id, true);
        renderAll();
      }
    }, 3000);
  }

  setInterval(function () {
    renderLink();
    // Ages and freshness move with the clock.
    renderList();
    renderRoom();
  }, 1000);

  RT.state = S;
  RT.select = select;
  RT.summaries = function () {
    return Array.from(rooms.values());
  };
  RT.socket = function () {
    return ws;
  };
  RT.linkState = linkState;

  renderAll();
  connect();
})();
