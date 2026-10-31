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

  function dbmText(dbm) {
    return typeof dbm === "number" && Number.isFinite(dbm) ? dbm.toFixed(1) : "—";
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

  const FAMILY_LABEL = { lsb: "LSB", usb: "USB", digl: "DIGL", digu: "DIGU", am: "AM", fm: "FM", cw: "CW", fdvl: "FreeDV L", fdvu: "FreeDV U" };

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
  RT.dbmText = dbmText;
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
        el("div", { class: "h" }, [el("span", { class: "c", text: callText(m.callsign, m.nth) }), el("span", { class: "t", text: (Number.isSafeInteger(m.hz) ? (m.hz / 1e6).toFixed(6) + " MHz · " : "Frequency not recorded · ") + hhmm(m.t) + " UTC" })]),
        el("div", { class: "x", text: goneText(m.state) }),
      ]);
    }
    return el("div", { class: "msg", data: { msg: m.id } }, [
      el("div", { class: "h" }, [
        el("span", { class: "c" }, RT.highlight(callText(m.callsign, m.nth))),
        typeof m.dbm === "number" ? el("span", { class: "hr", text: "reading " + sText(m.dbm) }) : null,
        el("span", { class: "t", text: (Number.isSafeInteger(m.hz) ? (m.hz / 1e6).toFixed(6) + " MHz · " : "Frequency not recorded · ") + hhmm(m.t) + " UTC" }),
      ]),
      el("div", { class: "x" }, formatted(m.text)),
    ]);
  }

  RT.messageNode = messageNode;


  // 2026-10-30: frequency-first original client, J.J. Boyd / KG4VCF, Codex.
  const modeFamily = {LSB:"lsb",USB:"usb",DIGL:"digl",DIGU:"digu",AM:"am",SAM:"am",FM:"fm",CWL:"cw",CWU:"cw",FDVL:"fdvl",FDVU:"fdvu"};
  const F = {
    contextId: c => c.family + "-" + c.hz,
    memberKey: m => m.sourceRoom ? m.sourceRoom + "/" + m.ref : m.ref,
    personKey: p => p.sourceRoom + "/" + p.ref + "/" + p.slice,
    family: mode => modeFamily[mode] || "",
    nearby: (item, c) => Number.isSafeInteger(item.hz) && item.hz > 0 && item.family === c.family && Math.abs(item.hz-c.hz) <= 5000,
    people: summaries => summaries.flatMap(r => (r.stations || []).filter(p => Number.isSafeInteger(p.hz) && p.hz > 0).map(p => Object.assign({},p,{band:r.band}))),
    groups: people => {
      const grouped = new Map();
      for (const p of people) {
        const key = F.contextId(p);
        if (!grouped.has(key)) grouped.set(key,{id:key,hz:p.hz,family:p.family,band:p.band,stations:[]});
        const stations=grouped.get(key).stations,existing=stations.find(station=>F.memberKey(station)===F.memberKey(p));
        if(existing){existing.slices.push(p.slice);existing.keyed=existing.keyed||p.keyed;existing.joining=existing.joining&&p.joining;}
        else stations.push(Object.assign({},p,{slices:[p.slice]}));
      }
      return Array.from(grouped.values()).sort((a,b)=>a.hz-b.hz || a.family.localeCompare(b.family));
    },
    lastTurn: (room,c) => {
      if (!room) return null;
      const inView = t => !Number.isSafeInteger(t.hz) || Math.abs(t.hz-c.hz)<=5000;
      const open = (room.now || []).filter(inView).slice().sort((a,b)=>Math.abs((a.hz||c.hz)-c.hz)-Math.abs((b.hz||c.hz)-c.hz) || b.start-a.start);
      return open[0] || (room.earlier || []).filter(inView).slice().sort((a,b)=>(b.end||b.start)-(a.end||a.start))[0] || null;
    },
    readings: (room,turn) => {
      if (!room || !turn) return [];
      const talkers = new Set((turn.talkers || []).filter(t=>t.ref).map(F.memberKey));
      return room.members.filter(m=>m.reading && m.reading.turn===turn.id && !talkers.has(F.memberKey(m))).sort((a,b)=>{
        const ak=typeof a.km==="number",bk=typeof b.km==="number";
        return ak&&bk&&a.km!==b.km ? a.km-b.km : ak!==bk ? ak?-1:1 : a.callsign.localeCompare(b.callsign) || a.nth-b.nth;
      });
    },
    path: (km,bearing,unit,show) => ({
      distance: typeof km==="number" && Number.isFinite(km) ? Math.round(unit==="km"?km:km/1.609344).toLocaleString("en-US") : "",
      bearing: typeof km==="number" && Number.isInteger(bearing) && bearing>=0 && bearing<360 && show ? String(bearing).padStart(3,"0")+"°T" : "",
    }),
    postReason: state => {
      if (!state.me) return "Public visitor · read only. Sign in to follow your station.";
      if (state.link!=="live") return "Not live. Your draft stays here; nothing is queued.";
      if (state.version!==1) return "This server does not offer frequency chat. Read only.";
      if (!state.follow || !state.follow.live) return "Your station is not publishing a live frequency. Your draft stays here.";
      if (!state.follow.atRadio) return "AWAY · nobody is at the radio. Posting and naming are off.";
      if (!state.following || state.earlier) return "Browsing only. Follow your selected slice to post or name a turn.";
      const slice=state.follow.slices.find(s=>s.id===state.slice);
      if (!slice || slice.hz!==state.context.hz || F.family(slice.mode)!==state.context.family) return "Your selected slice changed frequency or mode. Your original draft stays here.";
      return "";
    },
    visibleMessage: (rect,bounds,height,visible,modal) => visible&&!modal&&bounds.width>0&&bounds.height>0&&Math.min(rect.bottom,bounds.bottom,height)-Math.max(rect.top,bounds.top,0)>=Math.min(40,rect.height)&&rect.right>bounds.left&&rect.left<bounds.right,
    validText: text => !text.trim() ? "Write a message first." : Array.from(text).length>500 ? "A message can have up to 500 Unicode code points." : text.split("\n").length>10 ? "A message can have up to 10 lines." : "",
    currentHistoryStates: (states,requested,current,known) => (states||[]).filter(status=>requested.has(status.id)&&known.has(status.id)&&requested.get(status.id)===current.get(status.id)),
    historyUpdates: (messages,states,allowShown) => {
      const known=new Map(messages.map(m=>[m.id,m])),updates=[];
      for(const status of states||[]){
        const old=known.get(status.id);
        if(!old||!["shown","deleted","removed","expired"].includes(status.state))continue;
        if(status.state==="shown"&&(!allowShown||typeof status.text!=="string"))continue;
        updates.push(Object.assign({},old,{state:status.state,text:status.state==="shown"?status.text:null}));
      }
      return updates;
    },
    mergeMessages: (before,after) => {
      const merged=new Map(before.map(m=>[m.id,m]));
      for (const m of after) merged.set(m.id,Object.assign({},merged.get(m.id)||{},m));
      return Array.from(merged.values()).sort((a,b)=>a.id-b.id);
    },
  };
  RT.frequency=F;
  function readPreference(key,fallback) { try { return localStorage.getItem(key) || fallback; } catch(e) { return fallback; } }
  RT.pathPreference={unit:readPreference("roundtable.distance","mi")==="km"?"km":"mi",bearing:readPreference("roundtable.bearing","true")!=="false"};
  RT.pathControls=function(changed) {
    const unit=el("select",{id:"distance-unit",attrs:{"aria-label":"Distance units"}},[
      el("option",{value:"mi",text:"Miles (mi)"}),el("option",{value:"km",text:"Kilometres (km)"}),
    ]);
    unit.value=RT.pathPreference.unit;
    const bearing=el("input",{type:"checkbox",id:"show-bearing",checked:RT.pathPreference.bearing});
    function save() {
      RT.pathPreference.unit=unit.value==="km"?"km":"mi"; RT.pathPreference.bearing=bearing.checked;
      try {localStorage.setItem("roundtable.distance",RT.pathPreference.unit);localStorage.setItem("roundtable.bearing",String(bearing.checked));} catch(e) { /* Display still changes without browser storage. */ }
      changed();
    }
    unit.addEventListener("change",save);bearing.addEventListener("change",save);
    return el("div",{class:"path-controls"},[el("label",{},["Distance ",unit]),el("label",{},[bearing," True bearing (°T)"])]);
  };
  RT.pathNode=function(km,bearing,known) {
    const p=F.path(known===false?null:km,bearing,RT.pathPreference.unit,RT.pathPreference.bearing);
    return el("span",{class:"path-value"},[p.distance ? p.distance+" "+RT.pathPreference.unit : "",p.bearing?el("small",{text:p.bearing}):null]);
  };

  // ============================================================ public page
  if (!document.body || document.body.dataset.page!=="public") return;
  const $=id=>document.getElementById(id);
  const rooms=new Map(), cache=new Map(), historyMessages=new Map(), historyOrder=[], drafts=new Map(), failures=new Map(), earlierOpen=new Map();
  const seen=new Set(), mentionSeen=new Set(), exposed=new Map(), owned=new Set(), historyRestored=new Set();
  let muted;
  try { muted=new Set(JSON.parse(readPreference("roundtable.muted","[]"))); } catch(e) { muted=new Set(); }
  const S={band:null,q:"",quiet:true,sort:"f",sel:null,phoneRoom:false,earlier:false,following:false,slice:null,me:null,follow:null,turn:null};
  let ws=null,version=0,negotiated=false,listGen=0,viewGen=0,view=null,link="connecting",everLive=false,lastHeard=0,lostAt=Date.now(),offset=0,attempt=0,retryTimer=null;
  let queued=false,renderedContext=null,ownerBusy=false,ownerSeenMs=0,pendingSeenMs=0,marking=false;
  const historyRevisions=new Map(),historyValidated=new Map();
  let historySerial=0,historyPending=null,historyStale=true,historyValidatedAt=0;
  let tokens=20,tokensAt=Date.now(),pauseUntil=0,fetching=new Set();
  const BACKOFF=[1000,2000,5000,10000,30000],rowsBox=$("rows"),roomBox=$("room"),q=$("q");
  function now(){return Date.now()+offset;}
  function context(id){const m=String(id||"").match(/^([a-z]+)-([1-9][0-9]{0,10})$/);return m?{family:m[1],hz:Number(m[2])}:null;}
  function bandOf(hz){const b=BANDS.find(b=>hz>=b[1]&&hz<=b[2]);return b?b[0]:"other";}
  function current(){return context(S.sel);}
  function snapshot(){return view&&view.id===S.sel?view:cache.get(S.sel)||null;}
  function people(){return F.people(Array.from(rooms.values()));}
  function baseCall(call){return String(call||"").toUpperCase().split("/").sort((a,b)=>b.length-a.length)[0]||"";}
  function mentions(m){const own=S.me&&S.me.station&&baseCall(S.me.station.callsign);return !!own&&!owned.has(m.id)&&!historyRestored.has(m.id)&&((m.text||"").toUpperCase().match(/@?[A-Z0-9/]+/g)||[]).some(word=>baseCall(word.replace(/^@/,""))===own);}
  function unread(m){return !owned.has(m.id)&&(mentions(m)?!mentionSeen.has(m.id)&&m.t>ownerSeenMs:!seen.has(m.id));}
  function gate(c){return F.postReason({me:S.me,follow:S.follow,slice:S.slice,following:S.following,earlier:S.earlier,context:c||current()||{},link:linkState(),version:version});}
  function draftKey(c,slice){return String(slice===undefined?S.slice:slice)+":"+F.contextId(c);}
  function send(message){if(ws&&ws.readyState===1)ws.send(JSON.stringify(message));}
  function watch(c){if(!c||!negotiated)return;if(version===1)send({type:"watchAt",hz:c.hz,family:c.family});else if(rooms.has(F.contextId(c)))send({type:"watch",room:F.contextId(c)});}
  function release(c){if(!c||!negotiated)return;if(version===1)send({type:"unwatchAt",hz:c.hz,family:c.family});else send({type:"unwatch",room:F.contextId(c)});}
  function retain(messages){
    for(const m of messages||[]){
      const old=historyMessages.get(m.id);
      if(!old||old.state!==m.state||old.text!==m.text){historyRevisions.set(m.id,(historyRevisions.get(m.id)||0)+1);historyRestored.delete(m.id);historyStale=true;}
      if(!old)historyOrder.push({id:m.id});
      historyMessages.set(m.id,Object.assign({},old||{},m));
    }
    while(historyMessages.size>200){const id=historyMessages.keys().next().value;historyMessages.delete(id);historyRevisions.delete(id);historyValidated.delete(id);historyRestored.delete(id);}
    trimHistoryOrder();
  }
  function trimHistoryOrder(){
    let dividers=0;
    const kept=historyOrder.slice().reverse().filter(item=>item.id?historyMessages.has(item.id):dividers++<200).reverse();
    historyOrder.splice(0,historyOrder.length,...kept);
  }
  function refreshHistory(){
    historyStale=true;
    if(!historyMessages.size||!negotiated||version!==1||linkState()!=="live"){schedule();return;}
    const ids=Array.from(historyMessages.keys()).slice(-200),clientId="history-"+(++historySerial);
    historyPending={clientId:clientId,revisions:new Map(ids.map(id=>[id,historyRevisions.get(id)])),ids:new Set(ids)};
    send({type:"history",clientId:clientId,ids:ids});schedule();
  }
  function receiveHistory(message){
    const pending=historyPending;
    if(!pending||message.clientId!==pending.clientId)return;
    historyPending=null;
    // A new message elsewhere must not invalidate the known IDs in this
    // reply. A changed matching ID, including a newer tombstone, wins.
    const states=F.currentHistoryStates(message.states,pending.revisions,historyRevisions,historyMessages);
    const updates=F.historyUpdates(Array.from(historyMessages.values()),states,true);
    const byId=new Map(updates.map(m=>[m.id,m]));
    for(const m of updates){
      const old=historyMessages.get(m.id);
      if(m.state==="shown"&&(old.state!==m.state||old.text!==m.text))historyRestored.add(m.id);
      historyMessages.set(m.id,m);historyValidated.set(m.id,historyRevisions.get(m.id));
    }
    for(const room of cache.values())room.messages=room.messages.map(m=>byId.get(m.id)||m);
    if(view)view.messages=view.messages.map(m=>byId.get(m.id)||m);
    historyStale=Array.from(historyMessages.keys()).some(id=>historyValidated.get(id)!==historyRevisions.get(id));
    if(updates.length)historyValidatedAt=Date.now();
    schedule();
  }
  function noteTime(t){if(typeof t==="number")offset=t-Date.now();}
  function take(){const t=Date.now();tokens=Math.min(20,tokens+(t-tokensAt)*40/60000);tokensAt=t;if(t<pauseUntil||tokens<1)return false;tokens--;return true;}
  async function fetchContext(c){
    if(!c)return;
    const id=F.contextId(c);
    if(fetching.has(id)||!take())return;
    fetching.add(id);
    try {
      const route=version===1?"frequency":"room";
      const r=await getJson("/v1/api/"+route+"/"+c.family+"/"+c.hz);
      if(r.status===429){pauseUntil=Date.now()+Math.max(1,Number(r.headers.get("Retry-After"))||5)*1000;return;}
      if(r.status===200&&r.body&&r.body.room){
        noteTime(r.body.time);
        const data=r.body.room;
        cache.set(id,data);retain(data.messages);
        if(S.sel===id&&!(view&&view.id===id)){schedule();}
      }
    } catch(e){/* The connection line and retained view describe a lost link. */}
    finally{fetching.delete(id);}
  }
  function connect(){
    // Keep the last advertised presentation while disconnected. Each new
    // socket must negotiate again before watch/release can send new kinds.
    clearTimeout(retryTimer);negotiated=false;historyPending=null;historyStale=true;
    const scheme=location.protocol==="https:"?"wss:":"ws:";
    let socket;
    try{socket=new WebSocket(scheme+"//"+location.host+"/v1/live");}catch(e){failed();return;}
    ws=socket;
    socket.addEventListener("open",()=>{if(ws===socket){attempt=0;send({type:"watchBand",band:"all",frequencyVersion:1});}});
    socket.addEventListener("message",event=>{if(ws!==socket)return;let packet;try{packet=JSON.parse(event.data);}catch(e){return;}lastHeard=Date.now();onMessage(packet);});
    socket.addEventListener("close",()=>{if(ws===socket){ws=null;failed();}});
    socket.addEventListener("error",()=>{/* close schedules one retry. */});
  }
  function failed(){historyPending=null;historyStale=true;if(link==="live"||everLive){link="reconnecting";if(!lostAt)lostAt=Date.now();}retryTimer=setTimeout(connect,BACKOFF[Math.min(attempt++,BACKOFF.length-1)]);schedule();}
  function linkState(){if((link==="reconnecting"||link==="connecting")&&lostAt&&Date.now()-lostAt>20000)return "offline";return link;}
  function onMessage(m){
    if(m.type==="rooms"){
      version=m.frequencyVersion===1?1:0;negotiated=true;rooms.clear();for(const r of m.rooms||[])rooms.set(r.id,r);listGen=m.gen;link="live";lostAt=0;everLive=true;
      if(!S.sel){const first=directoryGroups()[0]||Array.from(rooms.values()).sort((a,b)=>a.hz-b.hz)[0];if(first)select({hz:first.hz,family:first.family},false);}
      else watch(current());
      refreshHistory();schedule();
    }else if(m.type==="history"){
      if(negotiated&&version===1)receiveHistory(m);
    }else if(m.type==="roomsDelta"){
      if(m.gen!==listGen+1){send({type:"resync",room:null});return;}
      listGen=m.gen;for(const r of (m.add||[]).concat(m.change||[]))rooms.set(r.id,r);for(const id of m.remove||[])rooms.delete(id);schedule();
    }else if(m.type==="room"){
      if(m.id!==S.sel)return;
      if(version===1&&m.view!=="frequency")return;
      view={id:m.id,hz:m.hz,family:m.family,band:m.band,opening:m.opening,view:m.view,members:m.members||[],now:m.now||[],earlier:m.earlier||[],messages:m.messages||[]};
      viewGen=m.gen;cache.set(m.id,view);retain(view.messages);schedule();
    }else if(m.type==="delta"){
      if(!view||m.room!==view.id||(version===1&&m.view!=="frequency"))return;
      if(m.gen!==viewGen+1){send(Object.assign({type:"resync",room:view.id},version===1?{view:"frequency"}:{}));return;}
      viewGen=m.gen;
      const members=new Map(view.members.map(row=>[version===1?F.memberKey(row):row.ref,row]));
      for(const row of m.members||[])members.set(version===1?F.memberKey(row):row.ref,row);
      for(const key of m.left||[])members.delete(key);view.members=Array.from(members.values());
      const turns=new Map(view.now.concat(view.earlier).map(t=>[t.id,t]));for(const t of m.turns||[])turns.set(t.id,t);
      view.now=Array.from(turns.values()).filter(t=>t.end===null).sort((a,b)=>a.start-b.start);
      view.earlier=Array.from(turns.values()).filter(t=>t.end!==null&&t.end>=now()-1800000).sort((a,b)=>b.end-a.end);
      view.messages=F.mergeMessages(view.messages,m.messages||[]).slice(-50);retain(m.messages);
      if(m.closed){view=null;fetchContext(current());}schedule();
    }else if(m.type==="error"){
      if(m.about==="watch"||m.about==="watchAt")fetchContext(current());
      if(m.about==="watchAt")toast(m.reason||"Cannot follow this frequency right now.");
    }
  }
  function select(c,manual){
    if(!c||!FAMILY_LABEL[c.family]||!Number.isSafeInteger(c.hz)||c.hz<=0)return;
    if(manual){S.following=false;S.earlier=false;}
    const id=F.contextId(c);
    if(id!==S.sel){release(current());S.sel=id;S.turn=null;view=null;viewGen=0;historyOrder.push({divider:{hz:c.hz,family:c.family}});trimHistoryOrder();watch(c);fetchContext(c);refreshHistory();}
    else if(!view)fetchContext(c);
    if(manual&&matchMedia("(max-width:760px)").matches){S.phoneRoom=true;document.body.classList.add("in-room");}
    schedule();
  }
  function directoryGroups(){
    let groups=version===1?F.groups(people()):Array.from(rooms.values()).map(r=>Object.assign({},r,{stations:[],legacy:true}));
    groups=groups.filter(g=>(!S.band||g.band===S.band)&&(!S.q||groupMatches(g))&&(S.quiet||g.stations.some(p=>p.keyed)||g.id===S.sel));
    if(S.sort==="n")groups.sort((a,b)=>b.stations.length-a.stations.length||a.hz-b.hz);
    else if(S.sort==="t")groups.sort((a,b)=>(rooms.get(b.stations[0]&&b.stations[0].sourceRoom)||b).last-(rooms.get(a.stations[0]&&a.stations[0].sourceRoom)||a).last||a.hz-b.hz);
    return groups;
  }
  function groupMatches(g){const query=S.q.toUpperCase();if(/^[0-9.]+$/.test(query))return freqText(g.hz).startsWith(query)||String(g.hz).startsWith(query.replace(/\./g,""));return g.stations.some(p=>p.callsign.toUpperCase().includes(query)||(p.grid||"").toUpperCase().includes(query))||(g.talkers||[]).some(call=>call.toUpperCase().includes(query))||g.stations.some(p=>{const source=rooms.get(p.sourceRoom);return source&&((source.talkers||[]).some(call=>call.toUpperCase().includes(query))||source.lastMessage&&source.lastMessage.callsign.toUpperCase().includes(query));});}
  function highlight(text){if(!S.q||/^[0-9.]+$/.test(S.q))return String(text);const value=String(text),i=value.toUpperCase().indexOf(S.q.toUpperCase());return i<0?value:[value.slice(0,i),el("mark",{text:value.slice(i,i+S.q.length)}),value.slice(i+S.q.length)];}
  RT.highlight=highlight;
  function renderBands(){
    const all=version===1?people():Array.from(rooms.values());
    const box=$("band-buttons");clear(box);
    const present=new Set(all.map(p=>p.band));
    for(const name of [null].concat(BANDS.map(b=>b[0]).filter(b=>ALWAYS.includes(b)||present.has(b)),present.has("other")?["other"]:[])){
      const list=name===null?all:all.filter(p=>p.band===name);
      const btn=el("button",{type:"button",class:"band"+(name===null?" all":"")+(S.band===name?" on":""),data:{band:name||""},disabled:name!==null&&!list.length,attrs:{"aria-pressed":String(S.band===name),"aria-label":(name===null?"All bands":bandLong(name))+", "+list.length+(version===1?" station slices":" frequencies")},on:{click:()=>{S.band=name;schedule();}}},[el("div",{class:"l"},[el("span",{class:"n",text:name===null?"All":bandShort(name)}),el("span",{class:"c",text:String(list.length)})]),el("div",{class:"act"})]);box.appendChild(btn);
    }
    $("quiet-count").textContent=String(people().filter(p=>!p.keyed).length);$("quiet-chip").classList.toggle("on",S.quiet);$("quiet-chip").setAttribute("aria-pressed",String(S.quiet));
  }
  function renderDirectory(){
    const keep=rowsBox.scrollTop;clear(rowsBox);
    const groups=directoryGroups();let band=null;
    for(const group of groups){
      if(group.band!==band){band=group.band;rowsBox.appendChild(el("div",{class:"grp"},[el("b",{class:"bn",text:bandLong(band)}),el("span",{class:"gc",text:version===1?"People at their reported frequencies":"Legacy frequency list · station positions unavailable"})]));}
      const section=el("section",{class:"frequency-group"+(group.id===S.sel?" selected":""),data:{frequency:group.id}});
      const label=el("button",{class:"frequency-label",type:"button",on:{click:()=>select(group,true)}},[freqNode("fq",group.hz),el("span",{class:"md",text:FAMILY_LABEL[group.family]||group.family}),group.stations.length>1?el("span",{class:"tag cochannel-label",text:group.stations.length+" cochannel"}):null]);
      section.appendChild(label);
      for(const person of group.stations){
        section.appendChild(el("button",{class:"person-row row"+(person.keyed?" keyed":""),type:"button",data:{id:group.id,person:F.personKey(person),hz:person.hz},attrs:{"aria-pressed":String(group.id===S.sel),"aria-label":"Browse "+callText(person.callsign,person.nth)+" at "+freqText(person.hz)+" Hz"},on:{click:()=>select(group,true)}},[
          glyph(person.keyed?"talking":"quiet"),el("span",{class:"person-call"},highlight(callText(person.callsign,person.nth))),el("span",{class:"person-grid",text:person.grid||""}),el("span",{class:"person-state",title:person.slices.length>1?person.slices.length+" slices at this exact frequency":"",text:(person.away?"AWAY":person.keyed?"Keyed":person.joining?"Joining":"Listening")+(person.slices.length>1?" · "+person.slices.length+" slices":"")}),el("span",{class:"browse-word",text:"Browse ›"}),
        ]));
      }
      if(group.legacy)section.appendChild(el("button",{class:"person-row row",type:"button",data:{id:group.id},on:{click:()=>select(group,true)}},[glyph(group.state||"quiet"),el("span",{class:"person-call",text:(group.talkers||[]).join(" + ")||"Quiet frequency"}),el("span",{class:"browse-word",text:"Read ›"})]));
      rowsBox.appendChild(section);
    }
    if(!groups.length)rowsBox.appendChild(el("div",{class:"empty-list",text:!everLive?"Connecting to Roundtable…":S.q?"No current station matches “"+S.q+"”. Search a frequency to browse its conversation.":"Nobody is publishing a station here right now."}));
    rowsBox.scrollTop=keep;$("shown").textContent=version===1?groups.reduce((n,g)=>n+g.stations.length,0)+" station positions · "+groups.length+" frequency groups":groups.length+" legacy frequencies";
  }
  function dialog(title,children){
    const old=$("frequency-details");if(old)old.remove();
    const box=el("dialog",{id:"frequency-details",class:"frequency-dialog"});
    const close=()=>{box.close();box.remove();};
    add(box,[el("h2",{text:title}),el("div",{class:"dialog-body"},children),el("button",{class:"btn small",type:"button",text:"Close",on:{click:close}})]);
    document.body.appendChild(box);box.addEventListener("cancel",()=>box.remove());box.showModal();return box;
  }
  function renderMap(){
    const box=$("bmap");clear(box);
    const c=current(),name=S.band||(c&&bandOf(c.hz));const band=BANDS.find(b=>b[0]===name);
    if(!band||version!==1){box.classList.add("hidden");return;}
    box.classList.remove("hidden");
    const width=Math.max(240,box.clientWidth-40),lo=band[1],hi=band[2];
    const groups=F.groups(people().filter(p=>p.band===name)).map(g=>({groups:[g],anchor:(g.hz-lo)/(hi-lo)*width,pinned:g.id===S.sel}));
    const clusters=[];
    for(const group of groups){const last=clusters[clusters.length-1];if(last&&!last.pinned&&!group.pinned&&group.anchor-last.anchor<112){last.groups.push(...group.groups);last.anchor=last.groups.reduce((n,g)=>n+(g.hz-lo)/(hi-lo)*width,0)/last.groups.length;}else clusters.push({groups:group.groups.slice(),anchor:group.anchor,pinned:group.pinned});}
    const capacity=Math.max(3,Math.floor(width/110));
    while(clusters.length>capacity){let best=-1,distance=Infinity;for(let i=0;i<clusters.length-1;i++){if(!clusters[i].pinned&&!clusters[i+1].pinned&&clusters[i+1].anchor-clusters[i].anchor<distance){best=i;distance=clusters[i+1].anchor-clusters[i].anchor;}}if(best<0)break;const a=clusters[best],b=clusters[best+1];a.groups.push(...b.groups);a.anchor=(a.anchor+b.anchor)/2;clusters.splice(best+1,1);}
    const tagWidth=Math.min(150,(width-Math.max(0,clusters.length-1)*6)/Math.max(1,clusters.length));
    let right=0;for(const g of clusters){g.left=Math.max(right,Math.min(width-tagWidth,g.anchor-tagWidth/2));right=g.left+tagWidth+6;}
    if(right>width+6){let edge=width;for(let i=clusters.length-1;i>=0;i--){clusters[i].left=Math.min(clusters[i].left,edge-tagWidth);edge=clusters[i].left-6;}}
    const inner=el("div",{class:"people-map"});
    inner.appendChild(el("div",{class:"map-caption",text:bandLong(name)+" · exact frequency ticks · Browse leaves your radio unchanged"}));
    const axis=el("div",{class:"axis"});for(let i=0;i<=4;i++){const tick=el("span",{text:((lo+(hi-lo)*i/4)/1e6).toFixed(3)});tick.style.left=(i/4)*100+"%";axis.appendChild(tick);}inner.appendChild(axis);
    for(const g of groups){const tick=el("span",{class:"frequency-tick",data:{hz:g.groups[0].hz},title:freqText(g.groups[0].hz)+" Hz"});tick.style.left=g.anchor+"px";inner.appendChild(tick);}
    for(const cluster of clusters){
      const folded=cluster.groups.length>1,group=cluster.groups[0],calls=group.stations.map(p=>callText(p.callsign,p.nth));
      const button=el("button",{class:"maptag"+(cluster.pinned?" selected":"")+(folded?" folded":"")+(group.stations.length>1&&!folded?" cochannel":""),type:"button",data:{groups:cluster.groups.map(g=>g.id).join(",")},attrs:{"aria-label":folded?"Browse "+cluster.groups.length+" crowded frequency groups":calls.join(", ")+" at "+freqText(group.hz)+" Hz"},on:{click:()=>{
        if(!folded){select(group,true);return;}
        const choices=cluster.groups.map(g=>el("button",{class:"fold-choice",type:"button",on:{click:()=>{select(g,true);$("frequency-details").close();$("frequency-details").remove();}}},[freqNode("fq",g.hz),el("span",{text:FAMILY_LABEL[g.family]}),el("b",{text:g.stations.map(p=>callText(p.callsign,p.nth)).join(", ")}),el("span",{text:"Browse"})]));
        dialog("People in crowded groups",choices);
      }}},folded?[el("b",{text:"+"+cluster.groups.length+" groups"}),el("small",{text:cluster.groups.reduce((n,g)=>n+g.stations.length,0)+" station positions · details"})]:[el("small",{text:(group.hz/1e6).toFixed(6)+" · "+FAMILY_LABEL[group.family]}),...calls.slice(0,3).map(call=>el("b",{text:call})),calls.length>3?el("small",{text:"+"+(calls.length-3)+" · Browse"}):null]);
      if(!folded&&group.stations.length>1&&group.id===S.sel){
        const currentRoom=snapshot(),turn=chosenTurn(currentRoom);
        if(turn){
          clear(button);add(button,[el("small",{text:(group.hz/1e6).toFixed(6)+" · "+FAMILY_LABEL[group.family]}),el("b",{text:"Last talker · "+turnLabel(turn)}),el("small",{text:"Heard by · dBm / age"})]);
          const reports=F.readings(currentRoom,turn).slice(0,2);
          for(const report of reports)add(button,el("small",{class:"stack-report",text:callText(report.callsign,report.nth)+" · "+dbmText(report.reading.dbm)+" / "+Math.max(0,Math.floor((now()-report.reading.t)/1000))+"s"}));
          if(!reports.length)button.appendChild(el("small",{text:group.stations.map(p=>p.callsign).join(", ")}));
        }
      }
      button.style.left=cluster.left+"px";button.style.width=tagWidth+"px";inner.appendChild(button);
      const leader=el("span",{class:"frequency-leader"});leader.style.left=Math.min(cluster.anchor,cluster.left+tagWidth/2)+"px";leader.style.width=Math.abs(cluster.anchor-(cluster.left+tagWidth/2))+"px";inner.appendChild(leader);
    }
    box.appendChild(inner);
  }
  async function pollOwner(){
    if(ownerBusy)return;ownerBusy=true;
    try{
      const signed=await me();
      if(!signed){S.me=null;S.follow=null;S.following=false;owned.clear();mentionSeen.clear();ownerSeenMs=0;pendingSeenMs=0;exposed.clear();return;}
      if(!S.me||S.me.csrf!==signed.csrf){owned.clear();mentionSeen.clear();ownerSeenMs=0;pendingSeenMs=0;exposed.clear();}
      S.me=signed;
      const result=await getJson("/v1/me/follow");
      if(result.status===401){S.me=null;S.follow=null;S.following=false;return;}
      if(result.status!==200||!result.body){S.follow=null;return;}
      S.follow=result.body;noteTime(result.body.time);
      ownerSeenMs=Math.max(ownerSeenMs,Number(result.body.mentionsSeenMs)||0);
      for(const id of result.body.ownMessageIds||[])owned.add(id);
      retain((result.body.mentions||[]).map(item=>item.message));
      const slices=result.body.slices||[];
      if(!slices.some(s=>s.id===S.slice))S.slice=slices.length?slices[0].id:null;
      if(S.following){const slice=slices.find(s=>s.id===S.slice);if(slice&&F.family(slice.mode))select({hz:slice.hz,family:F.family(slice.mode)},false);}
    }catch(e){S.follow=null;}
    finally{ownerBusy=false;schedule();}
  }
  function renderOwner(){
    const box=$("owner-bar");if(!box)return;
    if(box.contains(document.activeElement)&&document.activeElement.tagName==="SELECT")return;
    clear(box);
    if(!S.me){add(box,[el("span",{text:"Public frequency browsing · no radio controls"}),el("a",{class:"btn small",href:"/key",text:"Sign in"})]);return;}
    const slices=S.follow&&S.follow.slices||[];
    const selector=el("select",{id:"owner-slice",disabled:!slices.length,attrs:{"aria-label":"Station slice"}});
    for(const s of slices)selector.appendChild(el("option",{value:String(s.id),text:"Slice "+String.fromCharCode(65+s.id)+" · "+(s.hz/1e6).toFixed(6)+" MHz · "+s.mode+(s.keyed?" · keyed":"")}));
    if(!slices.length)selector.appendChild(el("option",{text:"No live station slice"}));else selector.value=String(S.slice);
    selector.addEventListener("change",()=>{S.slice=Number(selector.value);if(S.following){const s=slices.find(s=>s.id===S.slice);if(s)select({hz:s.hz,family:F.family(s.mode)},false);}schedule();});
    const follow=el("input",{id:"follow-station",type:"checkbox",checked:S.following,disabled:!slices.length||version!==1});
    follow.addEventListener("change",()=>{S.following=follow.checked;S.earlier=false;const s=slices.find(s=>s.id===S.slice);if(S.following&&s)select({hz:s.hz,family:F.family(s.mode)},false);schedule();});
    add(box,[el("span",{},["Signed in as ",el("b",{text:S.me.station.callsign||"your station"})]),selector,el("label",{},[follow," Follow my slice"]),el("a",{class:"btn small",href:"/archive",text:"Your archive"})]);
  }
  function turnLabel(turn){if(!turn)return "No attributed turn";if(turn.kind==="unknown")return "Unknown signal";if(turn.kind==="doubled")return (turn.talkers||[]).map(t=>callText(t.callsign,t.nth)).join(" + ")||"Two stations keyed";return (turn.talkers||[]).map(t=>callText(t.callsign,t.nth)).join(", ")||"Unidentified talker";}
  function chosenTurn(room){return room&&room.now.find(t=>t.id===S.turn)||F.lastTurn(room,current());}
  function namesNode(turn){return el("div",{class:"names"},(turn.names||[]).length?turn.names.map(n=>el("span",{class:"nm",text:n.name+" · "+n.count})):"No names suggested.");}
  function showName(turn){
    const c=current(),slice=S.slice,captured={slice:slice,hz:c.hz,family:c.family},why=gate(c);if(why){toast(why);return;}
    const field=el("input",{class:"field",type:"text",maxlength:16,autocomplete:"off",attrs:{"aria-label":"Callsign for this unknown turn"}}),status=el("p",{class:"err",attrs:{role:"status"}});
    const submit=el("button",{class:"btn glass",type:"submit",text:"Save name"});
    const form=el("form",{},[el("p",{text:"Unknown turn "+turn.id+" · "+freqText(turn.hz||c.hz)+" Hz. Only stations present during this turn may name it. Every suggestion stays visible."}),namesNode(turn),field,submit,status]);
    form.addEventListener("submit",async e=>{e.preventDefault();const currentReason=gate(c);if(currentReason){status.textContent=currentReason;return;}submit.disabled=true;try{const result=await postForm("/v1/me/nameAt",Object.assign({},captured,{csrf:S.me.csrf,turn:turn.id,name:field.value.trim().toUpperCase()}));if(result.status===200&&result.body&&result.body.ok){box.close();box.remove();fetchContext(c);}else status.textContent=reasonOf(result,"This station could not name that turn.");}catch(e){status.textContent="Naming was not confirmed. Try again when connected.";}finally{submit.disabled=false;}});
    const box=dialog("Name unknown turn",form);field.focus();
  }
  function readingNode(member,turn){
    const reading=member.reading,age=Math.max(0,now()-reading.t),known=turn.kind==="talking";
    return el("div",{class:"rd attributed-reading"+(age>20000?" stale":""),data:{ref:F.memberKey(member),turn:turn.id,km:typeof member.km==="number"?member.km:"",bearing:known&&Number.isInteger(member.bearingDeg)?member.bearingDeg:""},title:callText(member.callsign,member.nth)+" measured "+turnLabel(turn)+" at "+dbmText(reading.dbm)+" dBm"},[
      el("span",{class:"c"},[highlight(callText(member.callsign,member.nth)),member.away?el("span",{class:"away",text:"AWAY"}):null]),
      RT.pathNode(member.km,member.bearingDeg,known),el("span",{class:"o",text:(member.offsetHz>0?"+":"")+member.offsetHz}),
      el("span",{class:"v",text:sText(reading.dbm)+" / "+dbmText(reading.dbm)}),el("span",{class:"age",text:Math.floor(age/1000)+" s"}),
    ]);
  }
  function renderTalk(body,room){
    const turn=chosenTurn(room),open=turn&&turn.end===null;
    body.appendChild(el("div",{class:"cap"},["LAST TALKER",el("span",{text:turn?(open?"on the air":"turn ended "+ago(now()-turn.end)+" ago"):"at this viewed frequency"})]));
    body.appendChild(el("div",{class:"attributed-talker "+(turn?turn.kind:"quiet")},[glyph(turn?turn.kind:"quiet"),el("strong",{text:turnLabel(turn)}),turn&&turn.hz?el("span",{class:"mono",text:(turn.hz/1e6).toFixed(6)+" MHz"}):null]));
    if(!turn){body.appendChild(el("div",{class:"none",text:"No attributed turn near this frequency. Presence does not identify an unknown RF signal."}));return;}
    if(turn.kind==="unknown"){body.appendChild(namesNode(turn));body.appendChild(el("button",{class:"btn small name-turn",type:"button",text:"Name unknown turn",disabled:!!gate(),on:{click:()=>showName(turn)}}));}
    if(turn.kind==="doubled")body.appendChild(el("div",{class:"fn",text:"Combined RF is not credited to either talker or either archive. Distance and bearing are blank."}));
    const reads=F.readings(room,turn);
    const header=el("div",{class:"rd reading-head"},[el("span",{text:"Heard by"}),el("span",{text:RT.pathPreference.unit+(RT.pathPreference.bearing?" / °T":"")}),el("span",{class:"o",text:"Offset Hz"}),el("span",{text:"Signal · dBm"}),el("span",{text:"Age"})]);body.appendChild(header);
    for(const row of reads)body.appendChild(readingNode(row,turn));
    if(!reads.length)body.appendChild(el("div",{class:"none",text:"No current reporter reading belongs to this turn."}));
    body.appendChild(el("div",{class:"fn",text:turn.kind==="doubled"?"These are raw reports of the combined RF, not of either individual talker.":"Each row is that reporter’s raw reading of "+turnLabel(turn)+". Nearest first; distance and initial true bearing run from talker to reporter. Readings keep their original age after the turn ends."}));
    if(room.now.length>1){body.appendChild(el("div",{class:"activity-choices"},room.now.map(t=>el("button",{class:"chip"+(t.id===turn.id?" on":""),type:"button",text:turnLabel(t)+(t.hz?" · "+(t.hz/1e6).toFixed(6):""),on:{click:()=>{S.turn=t.id;schedule();}}}))));}
  }
  function earlierTurns(room){
    const key=S.sel,details=el("details",{class:"earlier-turns",open:earlierOpen.get(key)||false});details.addEventListener("toggle",()=>earlierOpen.set(key,details.open));details.appendChild(el("summary",{text:"Earlier turns · last 30 minutes"}));
    if(!room||!room.earlier.length){details.appendChild(el("div",{class:"none",text:"No earlier turns in this window."}));return details;}
    for(const turn of room.earlier){const row=el("div",{class:"turn",data:{turn:turn.id},attrs:{tabindex:"0"}},[el("span",{class:"t",text:hhmm(turn.start)}),glyph(turn.kind),el("span",{},[el("b",{class:"c",text:turnLabel(turn)}),el("small",{text:turn.hz?" · "+(turn.hz/1e6).toFixed(6)+" MHz":""})]),el("span",{class:"d",text:mmss(((turn.end||now())-turn.start)/1000)})]);details.appendChild(row);if(turn.kind==="unknown"){details.appendChild(namesNode(turn));details.appendChild(el("button",{class:"btn small name-turn",type:"button",text:"Name",disabled:!!gate(),on:{click:()=>showName(turn)}}));}}
    return details;
  }
  function saveMute(){try{localStorage.setItem("roundtable.muted",JSON.stringify(Array.from(muted)));}catch(e){/* Browser-local mute still works in this tab. */}}
  async function deleteOwn(m){
    if(!S.me||!owned.has(m.id))return;
    try{const result=await postForm("/v1/me/delete",{csrf:S.me.csrf,msg:m.id});if(result.status===200&&result.body&&result.body.ok){retain([Object.assign({},m,{state:"deleted",text:null})]);if(view)view.messages=F.mergeMessages(view.messages,[Object.assign({},m,{state:"deleted",text:null})]);schedule();}else toast(reasonOf(result,"This message could not be deleted."));}catch(e){toast("Deletion was not confirmed. Reconnect and try again.");}
  }
  function publicMessage(m){
    const node=messageNode(m),isMuted=muted.has(baseCall(m.callsign)),isOwn=owned.has(m.id);
    node.dataset.msg=String(m.id);node.classList.toggle("me",isOwn);node.classList.toggle("new-message",unread(m));node.classList.toggle("mention",mentions(m));
    if(unread(m))node.querySelector(".h").appendChild(el("small",{class:"new-label",text:"new"}));
    if(isMuted&&!isOwn&&m.state==="shown"){const content=node.querySelector(".x");clear(content);content.textContent="Messages from "+m.callsign+" are muted in this browser.";}
    const actions=el("div",{class:"message-actions"});
    if(isOwn&&m.state==="shown")actions.appendChild(el("button",{class:"linkbtn",type:"button",text:"Delete",on:{click:()=>deleteOwn(m)}}));
    if(!isOwn){actions.appendChild(el("button",{class:"linkbtn",type:"button",text:isMuted?"Unmute":"Mute",on:{click:()=>{const call=baseCall(m.callsign);if(muted.has(call))muted.delete(call);else muted.add(call);saveMute();schedule();}}}));}
    if(m.state==="shown")actions.appendChild(el("button",{class:"linkbtn",type:"button",text:"Copy",on:{click:async()=>{try{await navigator.clipboard.writeText(m.text);toast("Message copied.");}catch(e){toast("Copy is unavailable in this browser.");}}}}));
    node.appendChild(actions);return node;
  }
  function shownMessages(room){return S.earlier?historyOrder.map(entry=>entry.id?historyMessages.get(entry.id):entry).filter(Boolean):(room&&room.messages||[]).filter(m=>version!==1||F.nearby(m,current()));}
  function composer(c,existing){
    const key=draftKey(c),reason=gate(c);
    if(existing&&existing.dataset.key===key){
      const textarea=existing.querySelector("textarea"),button=existing.querySelector("button[type=submit]");
      textarea.disabled=!!reason;button.disabled=!!reason||!!F.validText(clean(textarea.value))||existing.dataset.sending==="yes";
      existing.querySelector(".audience").textContent=reason||"To stations within ±5,000 Hz in "+FAMILY_LABEL[c.family]+" · your slice stays on "+(c.hz/1e6).toFixed(6)+" MHz";
      return existing;
    }
    const captured={slice:S.slice,hz:c.hz,family:c.family},form=el("form",{class:"frequency-composer",data:{key:key}});
    const textarea=el("textarea",{rows:2,placeholder:reason?"Messages on this frequency":"Message nearby stations…",disabled:!!reason,attrs:{"aria-label":"Message on "+(c.hz/1e6).toFixed(6)+" MHz",maxlength:"4000"}});
    textarea.value=drafts.get(key)||"";
    const counter=el("div",{class:"counter"}),error=el("div",{class:"send-error",attrs:{role:"status"}}),submit=el("button",{class:"btn glass",type:"submit",text:"Send"});
    function update(){const value=textarea.value,count=Array.from(clean(value)).length;drafts.set(key,value);counter.textContent=count>=400?count+" / 500 code points"+(count>500?" · too long":""):"";const why=gate(c);textarea.disabled=!!why;submit.disabled=!!why||!!F.validText(clean(value))||form.dataset.sending==="yes";form.querySelector(".audience").textContent=why||"To stations within ±5,000 Hz in "+FAMILY_LABEL[c.family]+" · your slice stays on "+(c.hz/1e6).toFixed(6)+" MHz";clear(error);const failure=failures.get(key);if(failure)add(error,["Not sent · "+failure+" ",el("button",{class:"linkbtn",type:"button",text:"Try again",on:{click:()=>sendDraft()}})]);}
    async function sendDraft(){
      const text=clean(textarea.value),why=gate(c)||F.validText(text);
      if(why){failures.set(key,why);update();return;}
      // Freeze the actual accepted context; a retune cannot redirect this post.
      form.dataset.sending="yes";update();
      const clientId="web-"+(globalThis.crypto&&crypto.randomUUID?crypto.randomUUID():Date.now().toString(36)+"-"+Math.random().toString(36).slice(2));
      try{
        const result=await postForm("/v1/me/postAt",Object.assign({},captured,{csrf:S.me.csrf,text:text,clientId:clientId}));
        if(result.status===200&&result.body&&result.body.ok){owned.add(result.body.msg);failures.delete(key);if((drafts.get(key)||"")===textarea.value){drafts.delete(key);textarea.value="";}fetchContext(c);pollOwner();}
        else failures.set(key,reasonOf(result,"The server did not accept this message."));
      }catch(e){failures.set(key,"Delivery was not confirmed. Check the conversation before retrying.");}
      finally{form.dataset.sending="no";update();schedule();}
    }
    form.addEventListener("submit",e=>{e.preventDefault();sendDraft();});textarea.addEventListener("input",update);
    add(form,[el("div",{class:"audience"}),el("div",{class:"compose-row"},[textarea,submit]),counter,error]);update();return form;
  }
  function renderRoom(){
    const c=current(),room=snapshot(),old=roomBox.querySelector(".rbody"),oldComposer=roomBox.querySelector(".frequency-composer"),oldPath=roomBox.querySelector(".path-controls");
    const changed=renderedContext!==S.sel,atBottom=old&&old.querySelector(".msg")&&old.scrollHeight-old.clientHeight-old.scrollTop<36,keep=old?old.scrollTop:0;
    const focus=document.activeElement,focusId=focus&&focus.id,focusTurn=focus&&focus.dataset&&focus.dataset.turn,focusStart=focus&&focus.selectionStart,focusEnd=focus&&focus.selectionEnd;
    const oldEarlier=roomBox.querySelector(".earlier-turns");if(oldEarlier&&!changed)earlierOpen.set(S.sel,oldEarlier.open);
    clear(roomBox);
    roomBox.appendChild(el("button",{class:"ph-back",type:"button",text:"‹ All people",on:{click:()=>{S.phoneRoom=false;document.body.classList.remove("in-room");rowsBox.focus();}}}));
    if(!c){roomBox.appendChild(el("div",{class:"room-empty",text:everLive?"Browse a callsign or search a frequency to see who hears whom.":"Connecting to Roundtable…"}));return;}
    const family=el("select",{class:"tag family-select",id:"browse-family",attrs:{"aria-label":"Browse mode family"}},Object.keys(FAMILY_LABEL).map(key=>el("option",{value:key,text:FAMILY_LABEL[key]})));
    family.value=c.family;family.addEventListener("change",()=>select({hz:c.hz,family:family.value},true));
    const head=el("div",{class:"rhead"},[
      el("div",{class:"l1"},[freqNode("f",c.hz),family,el("button",{class:"btn small lnk",id:"copy-link",type:"button",text:"Copy link",on:{click:async()=>{const url="https://"+location.host+"/r/"+c.family+"/"+c.hz;try{await navigator.clipboard.writeText(url);toast("Copied "+url);}catch(e){toast(url);}window.__rtCopied=url;}}})]),
      el("div",{id:"conn",class:"conn "+linkState(),text:linkState()==="live"?"Live · "+(S.following?"following your selected slice":"browsing · radio unchanged"):linkState()==="offline"?"Offline since "+hhmm(lostAt||Date.now())+" UTC":linkState()==="reconnecting"?"Reconnecting · nothing new for "+Math.floor((Date.now()-lastHeard)/1000)+" s":"Connecting"}),
      el("p",{class:"frequency-range",text:"±5 kHz · "+((c.hz-5000)/1e6).toFixed(6)+"–"+((c.hz+5000)/1e6).toFixed(6)+" MHz · "+FAMILY_LABEL[c.family]}),
      oldPath||RT.pathControls(()=>schedule()),
    ]);
    if(everLive&&version!==1)head.appendChild(el("p",{class:"legacy-notice",text:"Legacy server · exact station positions and frequency chat are unavailable. This view is read only."}));
    roomBox.appendChild(head);
    const body=el("div",{class:"rbody",attrs:{tabindex:"-1","aria-label":"Frequency activity and conversation"}});
    if(room){renderTalk(body,room);body.appendChild(earlierTurns(room));}
    else body.appendChild(el("div",{class:"none",text:linkState()==="live"?"Loading this frequency…":"Last received conversation stays available while reconnecting."}));
    const tabs=el("div",{class:"conversation-tabs"},[
      el("button",{class:"chip"+(!S.earlier?" on":""),id:"current-conversation",type:"button",text:"Current conversation",attrs:{"aria-pressed":String(!S.earlier)},on:{click:()=>{S.earlier=false;schedule();}}}),
      el("button",{class:"chip"+(S.earlier?" on":""),id:"earlier-conversation",type:"button",text:"Earlier browsing",attrs:{"aria-pressed":String(S.earlier)},on:{click:()=>{refreshHistory();S.earlier=true;schedule();}}}),
    ]);body.appendChild(tabs);
    body.appendChild(el("div",{class:"cap"},[S.earlier?"RETAINED IN THIS TAB":"NEARBY CONVERSATION",el("span",{text:S.earlier?"original frequency and time · "+(historyStale||linkState()!=="live"?"status stale":"checked "+hhmm(historyValidatedAt)+" UTC"):"same family · inclusive 5,000 Hz"})]));
    const list=shownMessages(room),unseen=list.filter(m=>m.id&&unread(m)),named=unseen.filter(mentions);
    const jumps=el("div",{class:"message-jumps"});
    function jump(mention){const target=Array.from(body.querySelectorAll(".msg")).find(node=>unseen.some(m=>String(m.id)===node.dataset.msg&&(!mention||mentions(m))));if(target)target.scrollIntoView({block:"center",behavior:"smooth"});}
    if(named.length)jumps.appendChild(el("button",{class:"chip mention-jump",type:"button",text:"@"+named.length+" A message names you ↓",on:{click:()=>jump(true)}}));
    if(unseen.length&&!atBottom)jumps.appendChild(el("button",{class:"chip",type:"button",text:unseen.length+" new ↓",on:{click:()=>jump(false)}}));
    body.appendChild(jumps);
    if(!list.length)body.appendChild(el("div",{class:"none",text:S.earlier?"Messages you read remain here when you browse another frequency.":"No messages near this frequency yet."}));
    for(const item of list){if(item.divider)body.appendChild(el("div",{class:"context-divider",text:"Browsed "+(item.divider.hz/1e6).toFixed(6)+" MHz · "+FAMILY_LABEL[item.divider.family]+" · earlier messages retained"}));else body.appendChild(publicMessage(item));}
    roomBox.appendChild(body);roomBox.appendChild(composer(c,oldComposer));
    renderedContext=S.sel;body.scrollTop=changed?0:atBottom?body.scrollHeight:keep;
    if(focus&&roomBox.contains(focus)){focus.focus({preventScroll:true});if(typeof focusStart==="number"&&typeof focus.setSelectionRange==="function")focus.setSelectionRange(focusStart,focusEnd);}
    else if(focusTurn){const again=body.querySelector('.turn[data-turn="'+focusTurn+'"]');if(again)again.focus({preventScroll:true});}
    else if(focusId){const again=$(focusId);if(again)again.focus({preventScroll:true});}
  }
  async function markMentions(){
    if(marking||!S.me||pendingSeenMs<=ownerSeenMs)return;
    marking=true;const upTo=Math.min(pendingSeenMs,now());
    try{const result=await postForm("/v1/me/mentionsSeen",{csrf:S.me.csrf,t:upTo});if(result.status===200&&result.body&&result.body.ok){ownerSeenMs=Math.max(ownerSeenMs,upTo);schedule();}}catch(e){/* Seen intent stays local until a future successful request. */}
    finally{marking=false;}
  }
  function exposure(){
    if(document.visibilityState!=="visible"||document.querySelector("dialog[open]")){exposed.clear();return;}
    const body=roomBox.querySelector(".rbody");if(!body){exposed.clear();return;}
    const bounds=body.getBoundingClientRect(),t=Date.now(),eligible=new Set();
    if(bounds.width<=0||bounds.height<=0){exposed.clear();return;}
    for(const node of body.querySelectorAll(".msg[data-msg]")){
      const id=Number(node.dataset.msg),m=historyMessages.get(id);if(!m||!unread(m))continue;
      const rect=node.getBoundingClientRect();
      if(!F.visibleMessage(rect,bounds,innerHeight,true,false))continue;
      eligible.add(id);if(!exposed.has(id))exposed.set(id,t);
      if(t-exposed.get(id)>=2000){seen.add(id);if(mentions(m)){mentionSeen.add(id);pendingSeenMs=Math.max(pendingSeenMs,m.t);}node.classList.remove("new-message");const label=node.querySelector(".new-label");if(label)label.remove();}
    }
    for(const id of exposed.keys())if(!eligible.has(id))exposed.delete(id);
    markMentions();
  }
  function renderLink(){const state=linkState();document.querySelector(".page").classList.toggle("roundtable-offline",state==="offline"||state==="reconnecting");$("live").className="live "+state;$("live-state").textContent=state.toUpperCase();$("live-k").textContent=state==="live"?(version===1?people().length+" station slices · people on frequency":rooms.size+" legacy frequencies"):state==="reconnecting"?"nothing new for "+Math.floor((Date.now()-lastHeard)/1000)+" s":state==="offline"?"since "+hhmm(lostAt||Date.now())+" UTC":"";$("bands").classList.toggle("offline",state==="offline");$("bands").classList.toggle("connecting",state==="connecting");}
  function render(){renderBands();renderDirectory();renderMap();renderOwner();renderRoom();renderLink();const c=current();if(c&&location.pathname.startsWith("/r/")){const path="/r/"+c.family+"/"+c.hz;if(location.pathname!==path)history.replaceState(null,"",path);}}
  function schedule(){if(queued)return;queued=true;requestAnimationFrame(()=>{queued=false;render();});}
  function typedHz(text){const parts=text.split(".");if(parts.length>2)return Number(parts[0])*1e6+Number((parts[1]||"").padEnd(3,"0"))*1000+Number((parts.slice(2).join("")||"").padEnd(3,"0"));const n=Number(text);return text.includes(".")||text.length<=3?Math.round(n*1e6):text.length<=6?n*1000:n;}
  q.addEventListener("input",()=>{S.q=q.value.trim();if(/^[0-9]+(?:\.[0-9]+)*$/.test(S.q)){const hz=typedHz(S.q),near=F.groups(people()).sort((a,b)=>Math.abs(a.hz-hz)-Math.abs(b.hz-hz))[0];const family=near&&Math.abs(near.hz-hz)<=5000?near.family:current()?current().family:near?near.family:"usb";if(Number.isSafeInteger(hz)&&hz>0)select({hz:hz,family:family},true);}schedule();});
  $("quiet-chip").addEventListener("click",()=>{S.quiet=!S.quiet;schedule();});$("sort").addEventListener("change",e=>{S.sort=e.target.value;schedule();});
  window.addEventListener("resize",schedule);
  document.addEventListener("visibilitychange",()=>exposed.clear());
  document.addEventListener("keydown",e=>{
    const active=document.activeElement,input=active&&/INPUT|TEXTAREA|SELECT/.test(active.tagName);if(document.querySelector("dialog[open]"))return;
    if(e.key==="/"&&!input){e.preventDefault();q.focus();q.select();}
    else if(e.key==="Escape"&&(!input||active===q)){e.preventDefault();if(S.phoneRoom||active&&active.closest("#room")){S.phoneRoom=false;document.body.classList.remove("in-room");rowsBox.focus();}else if(S.q){S.q="";q.value="";rowsBox.focus();schedule();}else if(S.band){S.band=null;rowsBox.focus();schedule();}}
    else if((e.key==="ArrowDown"||e.key==="ArrowUp")&&(!input||active===q)&&!(active&&active.closest("#room"))){e.preventDefault();const all=directoryGroups(),at=all.findIndex(g=>g.id===S.sel),next=all[Math.max(0,Math.min(all.length-1,at+(e.key==="ArrowDown"?1:-1)))];if(next)select(next,true);}
    else if((e.key==="ArrowDown"||e.key==="ArrowUp")&&!input&&active&&active.closest("#room")){const turns=Array.from(roomBox.querySelectorAll(".turn"));if(turns.length){e.preventDefault();const at=turns.indexOf(active),index=at<0?(e.key==="ArrowDown"?0:turns.length-1):Math.max(0,Math.min(turns.length-1,at+(e.key==="ArrowDown"?1:-1)));const details=turns[index].closest("details");if(details)details.open=true;turns[index].focus();}}
    else if(e.key==="Enter"&&(active===rowsBox||active===q||active===document.body)){e.preventDefault();if(matchMedia("(max-width:760px)").matches){S.phoneRoom=true;document.body.classList.add("in-room");}const first=roomBox.querySelector(".turn"),body=roomBox.querySelector(".rbody");if(first){const details=first.closest("details");if(details)details.open=true;first.focus();}else if(body)body.focus();}
  });
  const path=location.pathname.match(/^\/r\/([a-z]+)\/([1-9][0-9]{0,10})$/);if(path&&FAMILY_LABEL[path[1]])S.sel=path[1]+"-"+path[2];
  RT.state=S;RT.select=id=>select(typeof id==="string"?context(id):id,true);RT.summaries=()=>Array.from(rooms.values());RT.socket=()=>ws;RT.currentView=()=>snapshot();RT.frequencyVersion=()=>version;RT.linkState=linkState;RT.render=render;RT.reconnectNow=()=>{if(!ws){attempt=0;connect();}};RT.roomLink=c=>"https://"+location.host+"/r/"+c.family+"/"+c.hz;
  render();connect();pollOwner();setInterval(pollOwner,5000);setInterval(()=>{if(historyMessages.size)refreshHistory();},10000);setInterval(()=>{renderLink();renderRoom();renderMap();},1000);setInterval(exposure,250);
})();
