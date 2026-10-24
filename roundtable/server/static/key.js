/* no-port-check: NereusSDR-original.
   The key page (design 7.3, 7.4): a website key for a station on other
   software, shown once; signing a browser in with a key; changing the
   key's callsign and grid; moving a key's own history to a new key. No
   email, no password. Only the key itself moves its history; nothing on
   this page moves history for someone else (D26). */
"use strict";

(function () {
  const RT = window.RT;
  const el = RT.el;
  const add = RT.add;
  const clear = RT.clear;

  const app = document.getElementById("app");

  function status() {
    return el("div", { attrs: { role: "status", "aria-live": "polite" } });
  }

  function say(node, ok, text) {
    clear(node);
    node.className = ok ? "ok" : "err";
    node.textContent = text;
  }

  function field(id, label, props) {
    const input = el("input", Object.assign({ class: "field", id: id, autocomplete: "off", spellcheck: false }, props || {}));
    return { input: input, label: el("label", { attrs: { for: id } }, [label, input]) };
  }

  /* A key shown once, with a copy button. */
  function shownKey(key, words, id) {
    const code = el("code", { id: id, text: key });
    const copy = el("button", { class: "btn small glass", type: "button", text: "Copy key", on: { click: async function () {
      try {
        await navigator.clipboard.writeText(key);
        RT.toast("Key copied");
      } catch (e) {
        RT.toast("Select the key and copy it");
      }
    } } });
    return el("div", { class: "shown-key" }, [
      el("p", { text: words }),
      code,
      copy,
      el("p", { text: "This is the only time it is shown. Roundtable keeps only a fingerprint of it, so it cannot show it again or send it to you." }),
    ]);
  }

  function newKeyCard() {
    const call = field("new-callsign", "Callsign", { placeholder: "W4XYZ" });
    const grid = field("new-grid", "Grid square", { placeholder: "EM73" });
    const out = status();
    const shown = el("div");
    const button = el("button", { class: "btn glass", type: "submit", text: "Get a key" });
    const form = el("form", { class: "form", id: "new-key", on: { submit: async function (e) {
      e.preventDefault();
      button.disabled = true;
      const r = await RT.postForm("/key/new", { callsign: call.input.value, grid: grid.input.value });
      button.disabled = false;
      clear(shown);
      if (r.status === 200 && r.body) {
        clear(out);
        out.className = "";
        shown.appendChild(shownKey(r.body.key, "Your key for " + r.body.callsign + (r.body.grid ? " in " + r.body.grid : "") + ". Put it in your program's Roundtable settings.", "new-key-shown"));
      } else {
        say(out, false, RT.reasonOf(r));
      }
    } } }, [el("span", { class: "grow" }, call.label), grid.label, button]);
    return el("section", { class: "card" }, [
      el("h2", { text: "Get a key" }),
      el("p", { text: "For a station on other software. One key is one station: your program sends it in place of the key NereusSDR keeps for itself. No email, no password." }),
      form,
      out,
      shown,
    ]);
  }

  function signInCard() {
    const key = field("signin-key", "Your key", { type: "password", placeholder: "43 characters" });
    const out = status();
    const button = el("button", { class: "btn glass", type: "submit", text: "Sign in" });
    const form = el("form", { class: "form", id: "signin", on: { submit: async function (e) {
      e.preventDefault();
      button.disabled = true;
      const r = await RT.postForm("/key/signin", { key: key.input.value.trim() });
      button.disabled = false;
      if (r.redirected || r.status === 303 || r.status === 200) {
        location.href = "/archive";
        return;
      }
      say(out, false, RT.reasonOf(r));
    } } }, [el("span", { class: "grow" }, key.label), button]);
    return el("section", { class: "card" }, [
      el("h2", { text: "Sign in with a key" }),
      el("p", { text: "Opens your archive in this browser. The key is sent once and is not kept in the browser." }),
      form,
      out,
    ]);
  }

  function profileCard() {
    const key = field("profile-key", "Your key", { type: "password" });
    const call = field("profile-callsign", "Callsign");
    const grid = field("profile-grid", "Grid square");
    const out = status();
    const button = el("button", { class: "btn", type: "submit", text: "Save" });
    const form = el("form", { class: "form", id: "profile", on: { submit: async function (e) {
      e.preventDefault();
      button.disabled = true;
      const r = await RT.postForm("/key/profile", { key: key.input.value.trim(), callsign: call.input.value, grid: grid.input.value });
      button.disabled = false;
      if (r.status === 200 && r.body) {
        say(out, true, "Saved: " + r.body.callsign + (r.body.grid ? " in " + r.body.grid : ", no grid") + ".");
      } else {
        say(out, false, RT.reasonOf(r));
      }
    } } }, [el("span", { class: "grow" }, key.label), call.label, grid.label, button]);
    return el("section", { class: "card" }, [
      el("h2", { text: "Change the callsign or grid" }),
      el("p", { text: "A key station's callsign and grid are set here. A callsign the program sends with its presence is not used." }),
      form,
      out,
    ]);
  }

  function moveCard() {
    const key = field("move-key", "Your current key", { type: "password" });
    const out = status();
    const shown = el("div");
    const confirm = el("input", { type: "checkbox", id: "move-confirm" });
    const button = el("button", { class: "btn warn", type: "submit", text: "Move my history to a new key", disabled: true });
    confirm.addEventListener("change", function () {
      button.disabled = !confirm.checked;
    });
    const form = el("form", { class: "form", id: "move", on: { submit: async function (e) {
      e.preventDefault();
      button.disabled = true;
      const r = await RT.postForm("/key/move", { key: key.input.value.trim() });
      clear(shown);
      if (r.status === 200 && r.body) {
        clear(out);
        out.className = "";
        key.input.value = "";
        confirm.checked = false;
        shown.appendChild(shownKey(r.body.key, "Your new key. Your history is on it now, and the old key has stopped working.", "moved-key-shown"));
      } else {
        button.disabled = !confirm.checked;
        say(out, false, RT.reasonOf(r));
      }
    } } }, [
      el("span", { class: "grow" }, key.label),
      el("label", { attrs: { for: "move-confirm" } }, [el("span", { text: "The old key stops working at once" }), el("span", {}, [confirm, " I understand"])]),
      button,
    ]);
    return el("section", { class: "card" }, [
      el("h2", { text: "Move my history to a new key" }),
      el("p", { text: "If your key may have been seen by someone else, move to a new one while you still hold it. Your sessions, turns, readings, names and messages move in one step and the old key is revoked." }),
      el("p", { text: "Only the key itself can do this. Nobody moves history for someone else, helpers included, and never by callsign. A key that is lost outright means a fresh start: a new key is a new station, and the old archive stays closed." }),
      form,
      out,
      shown,
    ]);
  }

  function waysIn(me) {
    return el("div", { class: "ways" }, [
      el("section", { class: "card" }, [
        el("h2", { text: "On NereusSDR" }),
        el("p", { text: "You need nothing here. NereusSDR keeps its own station key. The Archive button (the Roundtable panel menu, the phone's Roundtable page or the Spot Hub tab) opens a one-time link that signs this browser in." }),
      ]),
      el("section", { class: "card" }, [
        el("h2", { text: "On other software" }),
        el("p", { text: "Get a website key below and put it in your program. Sign in here with the same key to open your archive." }),
        me ? el("p", {}, ["This browser is signed in as ", el("b", { text: me.station.callsign || "" }), ". ", el("a", { href: "/archive", text: "Open your archive" })]) : null,
      ]),
    ]);
  }

  async function start() {
    const me = await RT.me();
    clear(app);
    add(app, [
      RT.topBar("Your station key", me ? me.station : null),
      el("main", { class: "keypage" }, [
        el("h2", { text: "Two ways into Roundtable" }),
        el("p", { text: "Roundtable has no email and no password. A station is its key." }),
        waysIn(me),
        newKeyCard(),
        signInCard(),
        profileCard(),
        moveCard(),
      ]),
    ]);
  }

  start();
})();
