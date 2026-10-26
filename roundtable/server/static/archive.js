/* no-port-check: NereusSDR-original.
   The archive page (design 11.6): a signed-in station's own sessions, who
   reported it and for how long, each station's raw readings on a
   timeline, an hourly histogram, and its messages. Reads GET /v1/me,
   /v1/me/sessions, /v1/me/session/<id> and /v1/me/histogram; posts
   /signout-everywhere and /archive/delete with the session's CSRF token. */
"use strict";

(function () {
  const RT = window.RT;
  const el = RT.el;
  const add = RT.add;
  const clear = RT.clear;

  const FIRST_LANES = 12;
  const DELETE_WORDS = "delete my archive";
  const DAY_MS = 86400000;
  const MONTHS = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
  const WEEKDAYS = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"];

  const S = { me: null, sessions: [], time: 0, sel: null, detail: null, lane: null, all: false, range: "30", band: "all", hist: null };
  const app = document.getElementById("app");

  function minutes(ms) {
    const m = Math.max(0, Math.round(ms / 60000));
    return m >= 60 ? Math.floor(m / 60) + "h " + String(m % 60).padStart(2, "0") + "m" : m + " min";
  }

  function secondsText(s) {
    if (s < 60) {
      return s + " s";
    }
    return minutes(s * 1000);
  }

  function dayLabel(ms) {
    const d = new Date(ms);
    const today = new Date(S.time || Date.now());
    const base = MONTHS[d.getUTCMonth()] + " " + d.getUTCDate();
    if (d.getUTCFullYear() !== today.getUTCFullYear()) {
      return base + ", " + d.getUTCFullYear();
    }
    if (RT.dateText(ms) === RT.dateText(today.getTime())) {
      return "TODAY · " + base;
    }
    return WEEKDAYS[d.getUTCDay()] + " · " + base;
  }

  function inRange(session) {
    if (S.band !== "all" && session.band !== S.band) {
      return false;
    }
    if (S.range === "all") {
      return true;
    }
    return session.start >= S.time - parseInt(S.range, 10) * DAY_MS;
  }

  function visible() {
    return S.sessions.filter(inRange);
  }

  // ------------------------------------------------------------ loading

  async function start() {
    const me = await RT.me();
    if (!me) {
      return signedOut();
    }
    S.me = me;
    S.time = me.time;
    const r = await RT.getJson("/v1/me/sessions");
    if (r.status !== 200 || !r.body) {
      return failed(RT.reasonOf(r));
    }
    S.time = r.body.time;
    S.sessions = r.body.sessions.slice().sort(function (a, b) {
      return b.start - a.start;
    });
    const wanted = new URLSearchParams(location.search).get("s");
    const first = S.sessions.find(function (s) {
      return String(s.id) === wanted;
    }) || visible()[0] || S.sessions[0];
    if (wanted && first) {
      S.range = "all";
    }
    render();
    if (first) {
      await open(first.id);
    }
    await histogram();
  }

  async function open(id) {
    S.sel = id;
    S.lane = null;
    S.all = false;
    S.detail = null;
    render();
    const r = await RT.getJson("/v1/me/session/" + id);
    if (S.sel !== id) {
      return;
    }
    S.detail = r.status === 200 ? r.body : { error: RT.reasonOf(r) };
    render();
  }

  async function histogram() {
    const params = new URLSearchParams();
    if (S.range !== "all") {
      params.set("days", S.range);
    }
    if (S.band !== "all") {
      params.set("band", S.band);
    }
    const r = await RT.getJson("/v1/me/histogram" + (params.toString() ? "?" + params.toString() : ""));
    S.hist = r.status === 200 && r.body ? r.body.hours : null;
    render();
  }

  // ------------------------------------------------------------ pages

  function signedOut() {
    clear(app);
    add(app, [
      RT.topBar("Your archive", null),
      el("div", { class: "center" }, [
        el("h2", { text: "Sign in to see your archive." }),
        el("p", { text: "Your archive is private to your station. Open it from the Archive button in NereusSDR on the desktop (the Roundtable panel's menu, or Spot Hub > Roundtable), which signs this browser in for you." }),
        el("p", {}, ["Stations on other software sign in with their website key on ", el("a", { href: "/key", text: "the key page" }), "."]),
      ]),
    ]);
  }

  function failed(reason) {
    clear(app);
    add(app, [RT.topBar("Your archive", S.me && S.me.station), el("div", { class: "center" }, [el("h2", { text: "The archive did not load." }), el("p", { text: reason })])]);
  }

  function seg(id, items, value, onPick) {
    return el("div", { class: "seg", id: id, attrs: { role: "group" } }, items.map(function (item) {
      return el("button", {
        type: "button",
        class: item[0] === value ? "on" : "",
        text: item[1],
        attrs: { "aria-pressed": item[0] === value ? "true" : "false" },
        on: { click: function () {
          onPick(item[0]);
        } },
      });
    }));
  }

  function render() {
    clear(app);
    const station = S.me.station;
    const top = RT.topBar("Your archive", station);
    app.appendChild(top);
    const bands = [];
    for (const s of S.sessions) {
      if (bands.indexOf(s.band) < 0) {
        bands.push(s.band);
      }
    }
    const bar = el("div", { class: "bar2" }, [
      el("span", { class: "lock", text: "Private to your station" }),
      seg("range", [["7", "7 days"], ["30", "30 days"], ["365", "Year"], ["all", "All"]], S.range, function (v) {
        S.range = v;
        render();
        histogram();
      }),
      seg("band", [["all", "All bands"]].concat(bands.map(function (b) {
        return [b, RT.bandLong(b)];
      })), S.band, function (v) {
        S.band = v;
        render();
        histogram();
      }),
      el("span", { class: "sp" }),
      el("button", { class: "btn small", type: "button", text: "Sign out everywhere", on: { click: signOutEverywhere } }),
    ]);
    app.appendChild(bar);
    app.appendChild(summary());
    const split = el("div", { class: "split" }, [sessionList(), detailPane()]);
    app.appendChild(split);
    if (S.lane) {
      drawSpark();
    }
  }

  function summary() {
    const vis = visible();
    const onAir = vis.reduce(function (a, s) {
      return a + (s.end - s.start);
    }, 0);
    const most = vis.reduce(function (a, s) {
      return Math.max(a, s.reported);
    }, 0);
    const tiles = [
      [String(vis.length), "SESSIONS"],
      [minutes(onAir), "ON THE AIR"],
      [String(vis.filter(function (s) {
        return s.reported > 0;
      }).length), "SESSIONS SOMEONE REPORTED"],
      [String(vis.filter(function (s) {
        return !s.reported;
      }).length), "SESSIONS NOBODY REPORTED"],
      [String(most), "MOST STATIONS IN ONE SESSION"],
    ];
    return el("div", { class: "sum" }, tiles.map(function (t) {
      return el("div", {}, [el("b", { text: t[0] }), el("span", { text: t[1] })]);
    }));
  }

  function sessionList() {
    const box = el("div", { class: "side", attrs: { "aria-label": "Sessions" } });
    const vis = visible();
    if (!vis.length) {
      box.appendChild(el("div", { class: "empty", text: S.sessions.length ? "No sessions in this range." : "No sessions yet. A session is kept each time you talk in a room." }));
      return box;
    }
    let lastDay = "";
    for (const s of vis) {
      const day = dayLabel(s.start);
      if (day !== lastDay) {
        box.appendChild(el("div", { class: "day", text: day }));
        lastDay = day;
      }
      box.appendChild(el("button", {
        type: "button",
        class: "s" + (s.reported ? "" : " nobody") + (s.id === S.sel ? " sel" : ""),
        data: { session: s.id },
        attrs: { "aria-pressed": s.id === S.sel ? "true" : "false" },
        on: { click: function () {
          open(s.id);
        } },
      }, [
        el("i", { class: "bar" }),
        el("span", { class: "f" }, [el("em", { text: RT.bandLong(s.band) }), s.hz ? RT.freqText(s.hz) : s.room]),
        el("span", { class: "w", text: RT.hhmm(s.start) + " to " + RT.hhmm(s.end) }),
        el("span", { class: "n" }, s.reported ? [el("b", { text: String(s.reported) }), s.reported === 1 ? " station reported you" : " stations reported you"] : "Nobody reported you"),
        el("span", { class: "dur", text: minutes(s.end - s.start) }),
      ]));
    }
    return box;
  }

  // ------------------------------------------------------------ one session

  /* Runs of one value over a per-second array. */
  function runs(values, test) {
    const out = [];
    let from = -1;
    for (let i = 0; i <= values.length; i += 1) {
      const on = i < values.length && test(values[i]);
      if (on && from < 0) {
        from = i;
      } else if (!on && from >= 0) {
        out.push([from, i]);
        from = -1;
      }
    }
    return out;
  }

  function span(trk, cls, from, to, total) {
    const node = el("i", { class: cls });
    node.style.left = (from / total) * 100 + "%";
    node.style.width = Math.max(0.15, ((to - from) / total) * 100) + "%";
    trk.appendChild(node);
  }

  function detailPane() {
    const box = el("div", { class: "det", attrs: { "aria-label": "Session" } });
    const s = S.sessions.find(function (x) {
      return x.id === S.sel;
    });
    if (!s) {
      box.appendChild(el("div", { class: "empty", text: "Pick a session." }));
      return box;
    }
    const d = S.detail;
    box.appendChild(el("div", { class: "sh" }, [
      el("span", { class: "f" }, [el("em", { text: RT.bandLong(s.band) }), s.hz ? RT.freqText(s.hz) : s.room]),
      el("span", { class: "w", text: RT.dateText(s.start) + " · " + RT.hhmm(s.start) + " to " + RT.hhmm(s.end) + " UTC · " + minutes(s.end - s.start) }),
      el("span", { class: "sp" }),
      s.family ? el("a", { href: "/r/" + s.family + "/" + s.hz, text: "Open this frequency's room" }) : null,
    ]));
    if (!d) {
      box.appendChild(el("div", { class: "none", text: "Loading…" }));
      return box;
    }
    if (d.error) {
      box.appendChild(el("div", { class: "err", text: d.error }));
      return box;
    }
    const t0 = s.start;
    const total = Math.max(1, (s.end - s.start) / 1000);
    const keyed = d.turns.reduce(function (a, t) {
      return a + (t.end - t.start);
    }, 0);
    if (!d.readers.length) {
      box.appendChild(el("div", { class: "emptybox" }, [
        el("b", { text: "Nobody reported you in this session." }),
        " You were keyed " + (d.turns.length === 1 ? "once" : d.turns.length + " times") + " over " + minutes(s.end - s.start) + " on " + RT.freqText(s.hz) + ". No station tuned there sent a reading of you. It is kept so your record shows the quiet times as well as the busy ones.",
      ]));
    }
    box.appendChild(el("div", { class: "cap" }, ["WHO HEARD YOU, AND FOR HOW LONG", el("span", { text: "Each row is that station's own raw readings. Nothing is averaged." })]));
    const tl = el("div", { class: "tl" });
    const ticks = el("div", { class: "ticks" });
    for (const f of [0, 0.25, 0.5, 0.75, 1]) {
      const tick = el("span", { text: RT.hhmm(t0 + f * total * 1000) });
      tick.style.left = f * 100 + "%";
      ticks.appendChild(tick);
    }
    tl.appendChild(el("div", { class: "lane hdr" }, [el("span", { text: "STATION" }), ticks, el("span", { class: "r", text: "WATCHED" }), el("span", { class: "r", text: "THEIR READINGS" })]));
    const you = el("div", { class: "trk" });
    for (const t of d.turns) {
      span(you, "tx", (t.start - t0) / 1000, (t.end - t0) / 1000, total);
    }
    tl.appendChild(el("div", { class: "lane you" }, [
      el("span", { class: "c", text: "You keyed" }),
      you,
      el("span", { class: "wd", text: minutes(keyed) }),
      el("span", { class: "rg", text: d.turns.length === 1 ? "once" : d.turns.length + " times" }),
    ]));
    const starts = {};
    for (const t of d.turns) {
      starts[t.id] = t.start;
    }
    const lanes = S.all ? d.readers : d.readers.slice(0, FIRST_LANES);
    lanes.forEach(function (reader, index) {
      const trk = el("div", { class: "trk" });
      let lo = null;
      let hi = null;
      for (const track of reader.tracks) {
        const base = ((starts[track.turn] || t0) - t0) / 1000 + track.first;
        for (const r of runs(track.present, function (p) {
          return p;
        })) {
          span(trk, track.away ? "aw" : "ok", base + r[0], base + r[1], total);
        }
        for (const r of runs(track.present, function (p) {
          return !p;
        })) {
          span(trk, "lost", base + r[0], base + r[1], total);
        }
        track.dbm.forEach(function (v, i) {
          if (v !== null && track.present[i]) {
            lo = lo === null ? v : Math.min(lo, v);
            hi = hi === null ? v : Math.max(hi, v);
          }
        });
      }
      const range = lo === null ? "" : lo === hi ? RT.sText(lo) : RT.sText(lo) + " to " + RT.sText(hi);
      tl.appendChild(el("button", {
        type: "button",
        class: "lane" + (S.lane === index ? " sel" : ""),
        data: { lane: index },
        attrs: { "aria-pressed": S.lane === index ? "true" : "false", "aria-label": (reader.callsign || "") + ", watched " + secondsText(reader.watched) },
        on: { click: function () {
          S.lane = S.lane === index ? null : index;
          render();
        } },
      }, [
        el("span", { class: "c" }, [reader.callsign || "", reader.away ? el("span", { class: "away", text: "AWAY" }) : null, reader.km !== null ? el("small", { text: reader.km.toLocaleString("en-US") + " km" }) : null]),
        trk,
        el("span", { class: "wd", text: secondsText(reader.watched) }),
        el("span", { class: "rg", text: range }),
      ]));
    });
    box.appendChild(tl);
    if (d.readers.length > FIRST_LANES) {
      box.appendChild(el("div", { class: "more" }, el("button", {
        class: "btn small",
        type: "button",
        id: "show-all",
        text: S.all ? "Show the first " + FIRST_LANES : "Show all " + d.readers.length,
        on: { click: function () {
          S.all = !S.all;
          render();
        } },
      })));
    }
    box.appendChild(el("div", { class: "legend" }, [
      el("span", {}, [el("i", { class: "tx" }), "you keyed"]),
      el("span", {}, [el("i", { class: "ok" }), "they reported you"]),
      el("span", {}, [el("i", { class: "lost" }), "you were keyed, they had lost you"]),
      el("span", {}, [el("i", { class: "aw" }), "reported while AWAY"]),
    ]));
    if (S.lane !== null && d.readers[S.lane]) {
      const reader = d.readers[S.lane];
      box.appendChild(el("div", { class: "spark", id: "spark" }, [
        el("div", { class: "h" }, [
          el("b", { text: reader.callsign || "" }),
          el("span", { text: "its raw S-meter readings of you, " + RT.hhmm(s.start) + " to " + RT.hhmm(s.end) + " UTC" }),
          el("button", { class: "linkbtn", type: "button", text: "Close", on: { click: function () {
            S.lane = null;
            render();
          } } }),
        ]),
        el("canvas", { id: "spk", width: 900, height: 112, attrs: { role: "img", "aria-label": "Readings of you by " + (reader.callsign || "") } }),
      ]));
    }
    box.appendChild(histogramNode());
    box.appendChild(messagesNode(d));
    box.appendChild(dangerZone());
    return box;
  }

  function drawSpark() {
    const canvas = document.getElementById("spk");
    const s = S.sessions.find(function (x) {
      return x.id === S.sel;
    });
    const d = S.detail;
    if (!canvas || !s || !d || !d.readers || !d.readers[S.lane]) {
      return;
    }
    const reader = d.readers[S.lane];
    const g = canvas.getContext("2d");
    const W = canvas.width;
    const H = canvas.height;
    const total = Math.max(1, (s.end - s.start) / 1000);
    const y = function (dbm) {
      return H - ((dbm + 127) / 80) * H;
    };
    g.fillStyle = "#151518";
    g.fillRect(0, 0, W, H);
    g.strokeStyle = "#29292c";
    g.lineWidth = 1;
    for (const level of [-109, -97, -85, -73]) {
      g.beginPath();
      g.moveTo(0, y(level));
      g.lineTo(W, y(level));
      g.stroke();
    }
    for (const t of d.turns) {
      g.fillStyle = "rgba(122, 44, 46, .35)";
      g.fillRect(((t.start - s.start) / 1000 / total) * W, 0, ((t.end - t.start) / 1000 / total) * W, H);
    }
    const starts = {};
    for (const t of d.turns) {
      starts[t.id] = t.start;
    }
    for (const track of reader.tracks) {
      const base = ((starts[track.turn] || s.start) - s.start) / 1000 + track.first;
      g.strokeStyle = track.away ? "#7f9a8c" : "#4caf6a";
      g.lineWidth = 2;
      let drawing = false;
      g.beginPath();
      track.dbm.forEach(function (v, i) {
        const x = ((base + i) / total) * W;
        if (v === null || !track.present[i]) {
          drawing = false;
          if (v === null) {
            return;
          }
          g.fillStyle = "rgba(216, 165, 95, .25)";
          g.fillRect(x, 0, Math.max(1, W / total), H);
          return;
        }
        if (!drawing) {
          g.moveTo(x, y(v));
          drawing = true;
        } else {
          g.lineTo(x, y(v));
        }
      });
      g.stroke();
    }
    g.fillStyle = "#a6a6ac";
    g.font = "20px Menlo, monospace";
    g.fillText("S9", 6, y(-73) - 4);
    g.fillText("S5", 6, y(-97) - 4);
  }

  function histogramNode() {
    const box = el("div", {});
    const label = (S.range === "all" ? "all time" : S.range === "365" ? "past year" : "past " + S.range + " days") + ", " + (S.band === "all" ? "all bands" : RT.bandLong(S.band));
    box.appendChild(el("div", { class: "cap" }, ["STATIONS THAT HEARD YOU, BY HOUR (UTC)", el("span", { text: label })]));
    const hist = el("div", { class: "hist", id: "hist", attrs: { role: "img", "aria-label": "Stations that heard you in each hour of the day" } });
    const hours = S.hist || new Array(24).fill(0);
    const max = Math.max.apply(null, hours.concat([1]));
    hours.forEach(function (v, i) {
      const bar = el("div", { class: v ? "" : "z", title: String(i).padStart(2, "0") + ":00 UTC, " + v + (v === 1 ? " station" : " stations"), data: { hour: i, count: v } });
      bar.style.height = Math.max((v / max) * 100, 1.5) + "%";
      hist.appendChild(bar);
    });
    box.appendChild(hist);
    box.appendChild(el("div", { class: "hx" }, ["00", "06", "12", "18", "23"].map(function (h) {
      return el("span", { text: h });
    })));
    return box;
  }

  function messagesNode(d) {
    const box = el("div", {});
    const expired = d.messages.filter(function (m) {
      return m.state === "expired";
    }).length;
    box.appendChild(el("div", { class: "cap" }, ["MESSAGES IN THIS SESSION", el("span", { text: expired ? "text removed after a year" : "text kept a year from the day sent" })]));
    const list = el("div", { class: "msgs" });
    if (expired) {
      list.appendChild(el("div", { class: "gonebox", text: "Message text is removed a year after it was sent. Who posted and when is still listed; the session and readings stay." }));
    }
    if (!d.messages.length) {
      list.appendChild(el("div", { class: "gonebox", text: "No messages were posted on this frequency while you were on." }));
    }
    for (const m of d.messages) {
      const node = RT.messageNode(m);
      if (m.mine) {
        node.classList.add("me");
      }
      list.appendChild(node);
    }
    box.appendChild(list);
    return box;
  }

  // ------------------------------------------------------------ account actions

  async function signOutEverywhere() {
    const r = await RT.postForm("/signout-everywhere", { csrf: S.me.csrf });
    if (r.redirected || r.status === 200) {
      location.href = "/";
      return;
    }
    RT.toast(RT.reasonOf(r));
  }

  function dangerZone() {
    const input = el("input", { class: "field", id: "delete-confirm", type: "text", autocomplete: "off", attrs: { "aria-label": "Type delete my archive to confirm" } });
    const message = el("div", { attrs: { role: "status" } });
    const button = el("button", { class: "btn danger small", type: "button", text: "Delete my archive", disabled: true });
    input.addEventListener("input", function () {
      button.disabled = input.value !== DELETE_WORDS;
    });
    button.addEventListener("click", async function () {
      button.disabled = true;
      const r = await RT.postForm("/archive/delete", { csrf: S.me.csrf, confirm: input.value });
      clear(message);
      if (r.status === 200 && r.body) {
        message.className = "ok";
        message.textContent = r.body.notice;
        S.sessions = [];
        S.sel = null;
        S.detail = null;
        setTimeout(render, 2500);
      } else {
        message.className = "err";
        message.textContent = RT.reasonOf(r);
        button.disabled = input.value !== DELETE_WORDS;
      }
    });
    return el("div", { class: "danger-zone" }, [
      el("b", { text: "Delete my archive" }),
      el("div", { text: "Deletes every session, reading and message of your station's archive. Backups age out within 7 days. To confirm, type “" + DELETE_WORDS + "”." }),
      el("div", { class: "row2" }, [input, button]),
      message,
    ]);
  }

  start();
})();
