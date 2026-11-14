/* no-port-check: NereusSDR-original.
   The review list (design 11.7, 12.2, 12.3): open reports oldest first,
   their actions (remove a message, block from posting, stop counting,
   leave it), Undo for 10 minutes, the blocked and not-counted stations
   with their lifts, the helpers, and the helper log. Every POST carries
   the session's CSRF token; the server checks the Origin. Nothing here
   moves history or signs anyone out (D26). */
"use strict";

(function () {
  const RT = window.RT;
  const el = RT.el;
  const add = RT.add;
  const clear = RT.clear;

  const UNDO_MS = 600000;
  const DAY_MS = 86400000;
  const REASON_TEXT = {
    spam: "Spam", abuse: "Abuse", offTopic: "Off topic", other: "Other", wrongName: "Wrong name", falseReadings: "False readings",
  };
  const DURATIONS = [["24h", "24 hours", DAY_MS], ["7d", "7 days", 7 * DAY_MS], ["30d", "30 days", 30 * DAY_MS], ["forever", "For good", 0]];

  const S = {
    me: null, tab: "open", reports: [], olderThanDay: 0, time: 0, offset: 0, sel: null,
    done: {}, form: null, conflict: null, error: null, log: null, names: {}, lookup: null, lookupText: "",
  };
  const app = document.getElementById("app");

  function serverNow() {
    return Date.now() + S.offset;
  }

  function noteTime(ms) {
    if (typeof ms === "number") {
      S.time = ms;
      S.offset = ms - Date.now();
    }
  }

  function when(ms) {
    return RT.dateText(ms) + " " + RT.hhmm(ms) + " UTC";
  }

  function nameOf(id) {
    return S.names[id] || null;
  }

  function stationLabel(id) {
    const c = nameOf(id);
    return c ? c : "station " + id;
  }

  function roomText(roomId) {
    if (!roomId) {
      return "";
    }
    const parts = roomId.split("-");
    const hz = parseInt(parts[1], 10);
    return (RT.FAMILY_LABEL[parts[0]] || parts[0]) + " " + (hz ? RT.freqText(hz) : roomId);
  }

  function untilText(mark) {
    if (!mark) {
      return "";
    }
    return mark.until === null ? "for good" : "until " + when(mark.until);
  }

  // ------------------------------------------------------------ loading

  async function start() {
    const me = await RT.me();
    if (!me) {
      return closedPage("Sign in to open the review list.", "Helpers sign in the same way as for the archive: the Archive button in NereusSDR signs this browser in, or a website key on the key page.");
    }
    S.me = me;
    noteTime(me.time);
    const ok = await loadReports();
    if (ok) {
      render();
      loadLog();
    }
  }

  function closedPage(title, text) {
    clear(app);
    add(app, [
      RT.topBar("Review list", S.me && S.me.station),
      el("div", { class: "center" }, [el("h2", { text: title }), el("p", { text: text }), el("p", {}, [el("a", { href: "/key", text: "The key page" }), " explains both ways to sign in."])]),
    ]);
  }

  async function loadReports() {
    const r = await RT.getJson("/v1/review/reports");
    if (r.status === 403) {
      closedPage(RT.reasonOf(r, "Only Roundtable helpers can open the review list."), "The review list is for the helpers the maintainer names. Anyone can report a message, a turn name or a station from the program.");
      return false;
    }
    if (r.status === 401) {
      closedPage("Sign in to open the review list.", "Helpers sign in the same way as for the archive.");
      return false;
    }
    if (r.status !== 200 || !r.body) {
      S.error = RT.reasonOf(r);
      render();
      return false;
    }
    noteTime(r.body.time);
    S.reports = r.body.reports;
    S.olderThanDay = r.body.olderThanDay;
    for (const entry of S.reports) {
      if (entry.station && entry.station.callsign) {
        S.names[entry.station.id] = entry.station.callsign;
      }
    }
    const ids = S.reports.map(function (e) {
      return e.id;
    });
    const kept = S.done[S.sel] || (S.conflict && S.conflict.entry === S.sel);
    if (S.sel === null || (ids.indexOf(S.sel) < 0 && !kept)) {
      S.sel = ids.length ? ids[0] : null;
    }
    for (const entry of S.reports) {
      if (entry.target === "message" && entry.message && !entry.context) {
        loadContext(entry);
      }
    }
    return true;
  }

  async function loadLog() {
    const r = await RT.getJson("/v1/review/log");
    if (r.status === 200 && r.body) {
      noteTime(r.body.time);
      S.log = r.body;
      for (const h of r.body.helpers) {
        if (h.callsign) {
          S.names[h.id] = h.callsign;
        }
      }
      render();
    }
  }

  /* The messages just before a reported one, from the read API. */
  async function loadContext(entry) {
    const parts = entry.message.room.split("-");
    const r = await RT.getJson("/v1/api/room/" + parts[0] + "/" + parts[1] + "/messages?before=" + (entry.message.id + 1) + "&limit=4");
    if (r.status === 200 && r.body) {
      entry.context = r.body.messages.slice().reverse();
      render();
    }
  }

  // ------------------------------------------------------------ actions

  async function act(entry, kind, extra) {
    S.conflict = null;
    S.error = null;
    const fields = Object.assign({ csrf: S.me.csrf, report: entry.id, version: entry.version }, extra || {});
    const r = await RT.postForm("/v1/review/" + kind, fields);
    if (r.status === 200 && r.body) {
      S.done[entry.id] = { entry: entry, kind: kind, action: r.body.action, report: r.body.report, extra: extra || {} };
      S.form = null;
      await loadReports();
      await loadLog();
      render();
      return;
    }
    if (r.status === 409 && r.body && r.body.error === "changed") {
      // The entry stays in view with the message, even when the other
      // helper's action closed it.
      S.conflict = { entry: entry.id, item: entry, action: r.body.action, reason: r.body.reason };
      await loadReports();
      render();
      return;
    }
    S.error = RT.reasonOf(r);
    render();
  }

  async function undo(entryId) {
    const done = S.done[entryId];
    if (!done) {
      return;
    }
    S.error = null;
    const r = await RT.postForm("/v1/review/undo", { csrf: S.me.csrf, action: done.action.id, version: done.report.version });
    if (r.status === 200) {
      delete S.done[entryId];
      await loadReports();
      await loadLog();
      render();
      return;
    }
    if (r.status === 409 && r.body && r.body.error === "changed") {
      S.conflict = { entry: entryId, action: r.body.action, reason: r.body.reason };
    } else {
      S.error = RT.reasonOf(r);
    }
    render();
  }

  async function lift(kind, stationId) {
    S.error = null;
    const r = await RT.postForm("/v1/review/" + kind, { csrf: S.me.csrf, station: stationId });
    if (r.status === 200) {
      RT.toast((kind === "unblock" ? "Unblocked " : "Counting again: ") + stationLabel(stationId));
      await loadReports();
      await loadLog();
    } else {
      S.error = RT.reasonOf(r);
    }
    render();
  }

  async function helperChange(kind, stationId) {
    S.error = null;
    const r = await RT.postForm("/v1/review/helpers/" + kind, { csrf: S.me.csrf, station: stationId });
    if (r.status !== 200) {
      S.error = RT.reasonOf(r);
    }
    await loadLog();
    if (S.lookupText) {
      await lookup(S.lookupText);
    }
    render();
  }

  async function lookup(callsign) {
    S.lookupText = callsign;
    const r = await RT.getJson("/v1/review/stations?callsign=" + encodeURIComponent(callsign));
    if (r.status === 200 && r.body) {
      S.lookup = r.body.stations;
      for (const st of r.body.stations) {
        if (st.callsign) {
          S.names[st.id] = st.callsign;
        }
      }
    } else {
      S.lookup = [];
      S.error = RT.reasonOf(r);
    }
    render();
  }

  // ------------------------------------------------------------ pages

  function tabs() {
    const items = [["open", "Open"], ["blocked", "Blocked stations"], ["log", "Helper log"]];
    return el("div", { class: "seg", id: "tabs", attrs: { role: "tablist" } }, items.map(function (item) {
      const on = S.tab === item[0];
      return el("button", {
        type: "button",
        class: on ? "on" : "",
        data: { tab: item[0] },
        attrs: { role: "tab", "aria-selected": on ? "true" : "false" },
        on: { click: function () {
          S.tab = item[0];
          S.form = null;
          S.conflict = null;
          S.error = null;
          if (item[0] === "open") {
            S.done = {};
          }
          render();
        } },
      }, [item[1], item[0] === "open" && S.reports.length ? el("span", { class: "n", text: String(S.reports.length) }) : null]);
    }));
  }

  function render() {
    clear(app);
    const top = RT.topBar("Review list", S.me ? S.me.station : null);
    app.appendChild(top);
    const bar = el("div", { class: "bar2" }, [
      tabs(),
      S.tab === "open" ? el("span", { class: "acts note", id: "older", text: S.olderThanDay ? S.olderThanDay + (S.olderThanDay === 1 ? " report is" : " reports are") + " older than a day" : "None older than a day" }) : null,
      el("span", { class: "sp" }),
      el("span", { class: "lock", text: "Helpers only" }),
    ]);
    app.appendChild(bar);
    if (S.tab === "open") {
      app.appendChild(openPane());
    } else if (S.tab === "blocked") {
      app.appendChild(blockedPane());
    } else {
      app.appendChild(logPane());
    }
  }

  function entryTitle(entry) {
    if (entry.target === "message") {
      return entry.message ? entry.message.callsign : "Message";
    }
    if (entry.target === "turn") {
      return "Turn" + (entry.turn ? " at " + RT.hhmm(entry.turn.start) : "");
    }
    return entry.station ? entry.station.callsign || "Station" : "Station";
  }

  function entryLine(entry) {
    if (entry.target === "message" && entry.message) {
      return entry.message.state === "shown" ? RT.plainLine(entry.message.text) : RT.goneText(entry.message.state);
    }
    if (entry.target === "turn" && entry.turn) {
      const names = entry.turn.names.map(function (n) {
        return n.name + " by " + (n.callsign || "a station");
      });
      return "(turn name) " + (names.length ? "named " + names.join(", ") : "an unknown turn");
    }
    return "(station) its readings or keyed state";
  }

  function reasonsLine(entry) {
    const seen = [];
    for (const r of entry.reports) {
      if (r.reason && seen.indexOf(REASON_TEXT[r.reason] || r.reason) < 0) {
        seen.push(REASON_TEXT[r.reason] || r.reason);
      }
    }
    return seen.length ? seen.join(", ") : "no reason given";
  }

  function openPane() {
    const split = el("div", { class: "split" });
    const list = el("div", { class: "q", attrs: { "aria-label": "Open reports, oldest first" } });
    const shown = S.reports.slice();
    for (const id of Object.keys(S.done)) {
      const done = S.done[id];
      if (!shown.find(function (e) {
        return String(e.id) === id;
      })) {
        shown.push(done.entry);
      }
    }
    if (S.conflict && S.conflict.item && !shown.find(function (e) {
      return e.id === S.conflict.entry;
    })) {
      shown.push(S.conflict.item);
    }
    shown.sort(function (a, b) {
      return a.first - b.first;
    });
    if (!shown.length) {
      list.appendChild(el("div", { class: "empty", text: "Nothing to review." }));
    }
    const now = serverNow();
    for (const entry of shown) {
      const old = entry.first < now - DAY_MS;
      list.appendChild(el("button", {
        type: "button",
        class: "it" + (entry.id === S.sel ? " sel" : ""),
        data: { entry: entry.id },
        attrs: { "aria-pressed": entry.id === S.sel ? "true" : "false" },
        on: { click: function () {
          S.sel = entry.id;
          S.form = null;
          S.conflict = null;
          S.error = null;
          render();
        } },
      }, [
        el("span", { class: "c", text: entryTitle(entry) + (S.done[entry.id] ? " · done" : "") }),
        el("span", { class: "t" + (old ? " old" : ""), text: roomText(entry.room) + " · " + (old ? Math.floor((now - entry.first) / DAY_MS) + " d ago" : RT.hhmm(entry.first)) }),
        el("span", { class: "x", text: entryLine(entry) }),
        el("span", { class: "m" }, [el("b", { text: entry.count + (entry.count === 1 ? " report" : " reports") }), " · " + reasonsLine(entry)]),
      ]));
    }
    split.appendChild(list);
    const det = el("div", { class: "det" });
    const entry = shown.find(function (e) {
      return e.id === S.sel;
    });
    if (entry) {
      det.appendChild(entryDetail(entry));
    } else if (S.error) {
      det.appendChild(el("div", { class: "rv" }, el("div", { class: "conflict", text: S.error })));
    }
    split.appendChild(det);
    return split;
  }

  function entryDetail(entry) {
    const box = el("div", { class: "rv", id: "entry" });
    const done = S.done[entry.id];
    if (entry.target === "message") {
      box.appendChild(el("div", { class: "cap", text: "REPORTED MESSAGE, WITH WHAT CAME BEFORE IT" }));
      const card = el("div", { class: "card" });
      const context = entry.context || (entry.message ? [Object.assign({ nth: 1, dbm: null }, entry.message)] : []);
      for (const m of context) {
        const node = RT.messageNode(Object.assign({ nth: 1, dbm: null }, m));
        if (entry.message && m.id === entry.message.id) {
          node.classList.add("hit");
        }
        card.appendChild(node);
      }
      if (!context.length) {
        card.appendChild(el("div", { class: "none", text: "The message is no longer stored." }));
      }
      box.appendChild(card);
    } else if (entry.target === "turn") {
      box.appendChild(el("div", { class: "cap", text: "REPORTED TURN NAME" }));
      const t = entry.turn;
      const card = el("div", { class: "card" });
      if (t) {
        add(card, el("div", { class: "kv" }, [
          el("span", { text: "Turn" }), el("span", { text: t.kind + ", " + RT.hhmm(t.start) + (t.end ? " to " + RT.hhmm(t.end) : ", still open") + " UTC" }),
          el("span", { text: "Room" }), el("span", { text: roomText(t.room) }),
          el("span", { text: "Names given" }), el("span", { text: t.names.length ? t.names.map(function (n) {
            return n.name + " (by " + (n.callsign || n.station) + ")";
          }).join(", ") : "none" }),
        ]));
      }
      box.appendChild(card);
    } else {
      box.appendChild(el("div", { class: "cap", text: "REPORTED STATION" }));
    }
    box.appendChild(el("div", { class: "cap", text: "WHO REPORTED IT" }));
    box.appendChild(el("div", { class: "card reps" }, entry.reports.map(function (r) {
      return el("div", { class: "r" }, [
        el("span", { class: "c", text: r.callsign || "a station" }),
        r.reason ? el("span", { class: "why", text: REASON_TEXT[r.reason] || r.reason }) : el("span", { class: "none2", text: "no reason given" }),
        el("span", { class: "t", text: when(r.t) }),
      ]);
    })));
    if (entry.station) {
      const st = entry.station;
      box.appendChild(el("div", { class: "cap", text: entry.target === "station" ? "THE STATION" : "WHO SENT IT" }));
      box.appendChild(el("div", { class: "card" }, el("div", { class: "kv" }, [
        el("span", { text: "Callsign" }), el("span", { text: (st.callsign || "") + (st.grid ? " · " + st.grid : "") }),
        el("span", { text: "Station id" }), el("span", { class: "sid", text: st.id }),
        el("span", { text: "Posting" }), el("span", { text: st.blocked ? "blocked " + untilText(st.blocked) : "allowed" }),
        el("span", { text: "Readings" }), el("span", { text: st.notCounted ? "not counted " + untilText(st.notCounted) : "counted" }),
      ])));
    }
    if (S.conflict && S.conflict.entry === entry.id) {
      box.appendChild(conflictNode(S.conflict));
    }
    if (S.error) {
      box.appendChild(el("div", { class: "conflict", attrs: { role: "alert" }, text: S.error }));
    }
    if (done) {
      box.appendChild(doneNode(entry, done));
    } else if (S.reports.find(function (e) {
      return e.id === entry.id;
    })) {
      box.appendChild(actionsNode(entry));
      if (S.form && S.form.entry === entry.id) {
        box.appendChild(durationForm(entry, S.form.kind));
      }
    }
    return box;
  }

  function actionWords(action) {
    if (!action) {
      return "acted on it";
    }
    const d = action.detail || {};
    switch (action.action) {
      case "remove":
        return "removed the message";
      case "block":
        return "blocked the station from posting " + (d.forever ? "for good" : "until " + when(d.until));
      case "stop-counting":
        return "stopped counting the station " + (d.forever ? "for good" : "until " + when(d.until));
      case "leave":
        return "left it as it is";
      case "undo":
        return "undid an action (" + (d.undid || "") + ")";
      default:
        return action.action;
    }
  }

  function conflictNode(conflict) {
    const a = conflict.action;
    const text = a
      ? "Another helper acted on this report first: " + (a.helperCallsign || "a helper") + " " + actionWords(a) + " at " + RT.hhmm(a.t) + " UTC. Nothing was changed; the list now shows the report as it is."
      : conflict.reason || "Another helper acted on this report first.";
    return el("div", { class: "conflict", id: "conflict", attrs: { role: "alert" }, text: text });
  }

  function actionsNode(entry) {
    const acts = el("div", { class: "acts" });
    if (entry.target === "message") {
      acts.appendChild(el("button", { class: "btn danger small", type: "button", text: "Remove message", data: { act: "remove" }, disabled: !entry.message || entry.message.state !== "shown", on: { click: function () {
        act(entry, "remove");
      } } }));
    }
    if (entry.target !== "turn" && entry.station) {
      const name = entry.station.callsign || "this station";
      acts.appendChild(el("button", { class: "btn warn small", type: "button", text: "Block " + name + " from posting…", data: { act: "block" }, on: { click: function () {
        S.form = { entry: entry.id, kind: "block", pick: "30d" };
        render();
      } } }));
      acts.appendChild(el("button", { class: "btn warn small", type: "button", text: "Stop counting " + name + "…", data: { act: "stop-counting" }, on: { click: function () {
        S.form = { entry: entry.id, kind: "stop-counting", pick: "30d" };
        render();
      } } }));
    }
    acts.appendChild(el("button", { class: "btn small", type: "button", text: "Leave it", data: { act: "leave" }, on: { click: function () {
      act(entry, "leave");
    } } }));
    acts.appendChild(el("span", { class: "note", text: "The reporters are told it was looked at, not what was done." }));
    const st = entry.station;
    if (st && (st.blocked || st.notCounted)) {
      const lifts = el("div", { class: "acts" });
      if (st.blocked) {
        lifts.appendChild(el("button", { class: "btn small", type: "button", text: "Unblock " + (st.callsign || ""), data: { act: "unblock" }, on: { click: function () {
          lift("unblock", st.id);
        } } }));
      }
      if (st.notCounted) {
        lifts.appendChild(el("button", { class: "btn small", type: "button", text: "Resume counting " + (st.callsign || ""), data: { act: "resume-counting" }, on: { click: function () {
          lift("resume-counting", st.id);
        } } }));
      }
      return el("div", {}, [acts, lifts]);
    }
    return acts;
  }

  function durationForm(entry, kind) {
    const name = (entry.station && entry.station.callsign) || "this station";
    const select = el("select", { class: "field", id: "duration", attrs: { "aria-label": "How long" } }, DURATIONS.map(function (d) {
      return el("option", { value: d[0], text: d[1], selected: S.form.pick === d[0] });
    }));
    select.addEventListener("change", function () {
      S.form.pick = select.value;
    });
    const title = kind === "block" ? "Block " + name + " from posting" : "Stop counting " + name;
    const hint = kind === "block"
      ? name + " can still listen, and its readings of other stations are still published. It cannot post or name turns while blocked."
      : "Its readings, presence and keyed state stop showing live and stop crediting turns. Its stored rows are kept and marked, so the signal record is never edited.";
    return el("div", { class: "blk", id: "duration-form" }, [
      el("b", { text: title }),
      el("div", { class: "row2" }, ["For ", select]),
      el("div", { class: "hint", text: hint }),
      el("div", { class: "acts" }, [
        el("button", { class: "btn warn small", type: "button", text: kind === "block" ? "Block" : "Stop counting", data: { act: kind + "-go" }, on: { click: function () {
          const pick = DURATIONS.find(function (d) {
            return d[0] === S.form.pick;
          });
          const until = pick[0] === "forever" ? "forever" : String(serverNow() + pick[2]);
          act(entry, kind, { until: until });
        } } }),
        el("button", { class: "btn small", type: "button", text: "Cancel", on: { click: function () {
          S.form = null;
          render();
        } } }),
      ]),
    ]);
  }

  function doneText(entry, done) {
    const name = (entry.station && entry.station.callsign) || "the station";
    const until = done.extra.until === "forever" ? "for good" : done.extra.until ? "until " + when(parseInt(done.extra.until, 10)) : "";
    switch (done.kind) {
      case "remove":
        return "Message removed. The room shows “Message removed” in its place.";
      case "block":
        return name + " is blocked from posting " + until + ".";
      case "stop-counting":
        return name + "'s readings and keyed state are not counted " + until + ".";
      default:
        return "Left as it is. Report closed.";
    }
  }

  function doneNode(entry, done) {
    const left = Math.max(0, done.action.t + UNDO_MS - serverNow());
    const button = el("button", { class: "btn small", type: "button", id: "undo", text: "Undo", disabled: left <= 0, on: { click: function () {
      undo(entry.id);
    } } });
    return el("div", { class: "done-b", id: "done" }, [
      el("span", { text: doneText(entry, done) }),
      button,
      el("span", { class: "cd", id: "undo-left", text: left > 0 ? "Undo possible for " + RT.mmss(left / 1000) : "Undo is possible for 10 minutes after an action." }),
    ]);
  }

  // ------------------------------------------------------------ blocked stations and helpers

  /* Stations with a block or stop-counting in effect, from the log: the
     latest such action not undone, not lifted since and not run out. */
  function marked() {
    const out = {};
    if (!S.log) {
      return [];
    }
    const now = serverNow();
    const actions = S.log.actions.slice().sort(function (a, b) {
      return a.id - b.id;
    });
    for (const a of actions) {
      const key = a.target;
      if (a.action === "block" || a.action === "stop-counting") {
        if (a.undone !== null) {
          continue;
        }
        const d = a.detail || {};
        out[key] = out[key] || { id: key, blocked: null, notCounted: null };
        const mark = { until: d.forever ? null : d.until, by: a.helperCallsign, t: a.t };
        if (a.action === "block") {
          out[key].blocked = mark;
        } else {
          out[key].notCounted = mark;
        }
      } else if ((a.action === "unblock" || a.action === "resume-counting") && out[key]) {
        if (a.action === "unblock") {
          out[key].blocked = null;
        } else {
          out[key].notCounted = null;
        }
      }
    }
    return Object.keys(out).map(function (k) {
      const v = out[k];
      if (v.blocked && v.blocked.until !== null && v.blocked.until <= now) {
        v.blocked = null;
      }
      if (v.notCounted && v.notCounted.until !== null && v.notCounted.until <= now) {
        v.notCounted = null;
      }
      return v;
    }).filter(function (v) {
      return v.blocked || v.notCounted;
    });
  }

  function blockedPane() {
    const det = el("div", { class: "det" });
    const box = el("div", { class: "rv" });
    if (S.error) {
      box.appendChild(el("div", { class: "conflict", attrs: { role: "alert" }, text: S.error }));
    }
    box.appendChild(el("div", { class: "cap" }, ["STATIONS BLOCKED OR NOT COUNTED", el("span", { text: "from the helper log" })]));
    const list = el("div", { class: "card blist", id: "blocked" });
    const rows = marked();
    if (!rows.length) {
      list.appendChild(el("div", { class: "none2", text: S.log ? "No station is blocked or uncounted." : "Loading…" }));
    }
    for (const v of rows) {
      const lines = [];
      if (v.blocked) {
        lines.push("blocked from posting " + untilText(v.blocked) + (v.blocked.by ? ", by " + v.blocked.by : ""));
      }
      if (v.notCounted) {
        lines.push("not counted " + untilText(v.notCounted) + (v.notCounted.by ? ", by " + v.notCounted.by : ""));
      }
      list.appendChild(el("div", { class: "b", data: { station: v.id } }, [
        el("span", { class: "c", text: nameOf(v.id) || "Station" }),
        el("span", {}, [el("small", { text: v.id })]),
        el("span", { class: "u", text: lines.join("; ") }),
        el("span", { class: "acts" }, [
          v.blocked ? el("button", { class: "btn small", type: "button", text: "Unblock", data: { act: "unblock" }, on: { click: function () {
            lift("unblock", v.id);
          } } }) : null,
          v.notCounted ? el("button", { class: "btn small", type: "button", text: "Resume counting", data: { act: "resume-counting" }, on: { click: function () {
            lift("resume-counting", v.id);
          } } }) : null,
        ]),
      ]));
    }
    box.appendChild(list);
    box.appendChild(el("div", { class: "cap" }, ["HELPERS", el("span", { text: "added and removed by station id" })]));
    const helpers = el("div", { class: "card blist", id: "helpers" });
    for (const h of S.log ? S.log.helpers : []) {
      helpers.appendChild(el("div", { class: "b" }, [
        el("span", { class: "c", text: h.callsign || "Station" }),
        el("span", {}, [el("small", { text: h.id })]),
        el("span", { class: "u2", text: h.config ? "set in the server's configuration" : h.added ? "helper since " + RT.dateText(h.added) : "" }),
        h.config ? el("span") : el("button", { class: "btn small", type: "button", text: "Remove", on: { click: function () {
          helperChange("remove", h.id);
        } } }),
      ]));
    }
    box.appendChild(helpers);
    box.appendChild(el("div", { class: "cap" }, ["FIND A STATION", el("span", { text: "by callsign; each station shows with its id" })]));
    const input = el("input", { class: "field", id: "find", type: "search", value: S.lookupText, placeholder: "Callsign", autocomplete: "off", attrs: { "aria-label": "Callsign" } });
    const form = el("form", { class: "form", on: { submit: function (e) {
      e.preventDefault();
      if (input.value.trim()) {
        lookup(input.value.trim());
      }
    } } }, [input, el("button", { class: "btn small", type: "submit", text: "Find" })]);
    box.appendChild(form);
    if (S.lookup) {
      const found = el("div", { class: "card blist", id: "found" });
      if (!S.lookup.length) {
        found.appendChild(el("div", { class: "none2", text: "No station has that callsign." }));
      }
      for (const st of S.lookup) {
        found.appendChild(el("div", { class: "b", data: { station: st.id } }, [
          el("span", { class: "c", text: st.callsign || "" }),
          el("span", {}, [el("small", { text: st.id })]),
          el("span", { class: "u2", text: (st.kind === "key" ? "website key" : "station key") + (st.grid ? " · " + st.grid : "") + " · seen " + RT.dateText(st.lastSeen) }),
          el("span", { class: "acts" }, [
            st.helper
              ? el("button", { class: "btn small", type: "button", text: "Remove helper", on: { click: function () {
                helperChange("remove", st.id);
              } } })
              : el("button", { class: "btn small", type: "button", text: "Add as helper", data: { act: "helper-add" }, on: { click: function () {
                helperChange("add", st.id);
              } } }),
            el("button", { class: "btn small", type: "button", text: "Unblock", on: { click: function () {
              lift("unblock", st.id);
            } } }),
            el("button", { class: "btn small", type: "button", text: "Resume counting", on: { click: function () {
              lift("resume-counting", st.id);
            } } }),
          ]),
        ]));
      }
      box.appendChild(found);
    }
    det.appendChild(box);
    return det;
  }

  // ------------------------------------------------------------ log

  function logWords(a) {
    const d = a.detail || {};
    switch (a.action) {
      case "remove":
        return "Removed a message" + (d.station ? " from " + stationLabel(d.station) : "") + (d.room ? " in " + roomText(d.room) : "");
      case "block":
        return "Blocked " + stationLabel(a.target) + " from posting " + (d.forever ? "for good" : "until " + when(d.until));
      case "stop-counting":
        return "Stopped counting " + stationLabel(a.target) + " " + (d.forever ? "for good" : "until " + when(d.until));
      case "leave":
        return "Left report " + a.target + " as it is";
      case "undo":
        return "Undid " + (d.undid || "an action") + " (action " + a.target + ")";
      case "unblock":
        return "Unblocked " + stationLabel(a.target);
      case "resume-counting":
        return "Resumed counting " + stationLabel(a.target);
      case "helper-add":
        return "Added " + stationLabel(a.target) + " as a helper";
      case "helper-remove":
        return "Removed " + stationLabel(a.target) + " as a helper";
      default:
        return a.action;
    }
  }

  function logPane() {
    const det = el("div", { class: "det" });
    const box = el("div", { class: "rv" });
    box.appendChild(el("div", { class: "cap", text: "EVERY ACTION, NEWEST FIRST" }));
    const card = el("div", { class: "card log", id: "log" });
    const actions = S.log ? S.log.actions : [];
    if (!actions.length) {
      card.appendChild(el("div", { class: "none2", text: S.log ? "No helper has acted yet." : "Loading…" }));
    }
    for (const a of actions) {
      card.appendChild(el("div", { class: "l", data: { action: a.action } }, [
        el("span", { class: "t", text: when(a.t) }),
        el("span", { class: "c", text: a.helperCallsign || "a helper" }),
        el("span", {}, [logWords(a), a.undone !== null ? el("span", { class: "u", text: " (undone)" }) : null]),
      ]));
    }
    box.appendChild(card);
    det.appendChild(box);
    return det;
  }

  // The Undo countdown moves with the clock.
  setInterval(function () {
    const node = document.getElementById("undo-left");
    if (!node || S.tab !== "open") {
      return;
    }
    const done = S.done[S.sel];
    if (!done) {
      return;
    }
    const left = Math.max(0, done.action.t + UNDO_MS - serverNow());
    node.textContent = left > 0 ? "Undo possible for " + RT.mmss(left / 1000) : "Undo is possible for 10 minutes after an action.";
    const button = document.getElementById("undo");
    if (button) {
      button.disabled = left <= 0;
    }
  }, 1000);

  start();
})();
