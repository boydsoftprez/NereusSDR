/* no-port-check: NereusSDR-original.
   The size and contrast check, run in the page by the page checks: every
   visible text is at least 11 px, and at least 4.5:1 against its ground,
   except text of a disabled control. The ground is composed from the
   element's ancestors (their background colors and every color stop of a
   gradient, worst case), with the opacity of the ancestors applied to the
   text. Evaluates to a list of failures. */
(function () {
  "use strict";
  function parse(text) {
    const m = /rgba?\(([^)]+)\)/.exec(text || "");
    if (!m) {
      return null;
    }
    const p = m[1].split(/[\s,\/]+/).filter(Boolean).map(parseFloat);
    return [p[0], p[1], p[2], p.length > 3 ? p[3] : 1];
  }
  function stops(image) {
    const out = [];
    const re = /rgba?\([^)]+\)/g;
    let m;
    while ((m = re.exec(image || "")) !== null) {
      out.push(parse(m[0]));
    }
    return out;
  }
  function over(top, bottom) {
    const a = top[3];
    return [
      top[0] * a + bottom[0] * (1 - a),
      top[1] * a + bottom[1] * (1 - a),
      top[2] * a + bottom[2] * (1 - a),
      1,
    ];
  }
  function lum(c) {
    const f = function (v) {
      v /= 255;
      return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2]);
  }
  function ratio(a, b) {
    const x = lum(a);
    const y = lum(b);
    return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05);
  }
  function grounds(el) {
    const chain = [];
    for (let n = el; n && n.nodeType === 1; n = n.parentElement) {
      chain.unshift(n);
    }
    let cands = [[255, 255, 255, 1]];
    for (const n of chain) {
      const cs = getComputedStyle(n);
      const g = stops(cs.backgroundImage);
      if (g.length) {
        const next = [];
        for (const c of cands) {
          for (const s of g) {
            next.push(over(s, c));
          }
        }
        cands = next.slice(0, 64);
      }
      const bg = parse(cs.backgroundColor);
      if (bg && bg[3] > 0) {
        cands = cands.map(function (c) {
          return over(bg, c);
        });
      }
    }
    return cands;
  }
  function opacity(el) {
    let o = 1;
    for (let n = el; n && n.nodeType === 1; n = n.parentElement) {
      o *= parseFloat(getComputedStyle(n).opacity);
    }
    return o;
  }
  function visible(el) {
    const cs = getComputedStyle(el);
    if (cs.visibility !== "visible" || cs.display === "none") {
      return false;
    }
    const r = el.getBoundingClientRect();
    if (r.width <= 1 || r.height <= 1) {
      return false;
    }
    for (let n = el; n && n.nodeType === 1; n = n.parentElement) {
      const s = getComputedStyle(n);
      if (s.display === "none" || parseFloat(s.opacity) === 0) {
        return false;
      }
    }
    return true;
  }
  function describe(el) {
    let s = el.tagName.toLowerCase();
    if (el.id) {
      s += "#" + el.id;
    }
    if (el.className && typeof el.className === "string") {
      s += "." + el.className.trim().split(/\s+/).join(".");
    }
    const p = el.parentElement;
    return (p ? (p.className && typeof p.className === "string" ? p.tagName.toLowerCase() + "." + p.className.trim().split(/\s+/).join(".") : p.tagName.toLowerCase()) + " > " : "") + s;
  }
  const fails = [];
  let checked = 0;
  const all = document.body.querySelectorAll("*");
  for (const el of all) {
    if (el.tagName === "SCRIPT" || el.tagName === "STYLE" || el.tagName === "OPTION") {
      continue;
    }
    let text = "";
    for (const c of el.childNodes) {
      if (c.nodeType === 3) {
        text += c.textContent;
      }
    }
    if (el.tagName === "INPUT" && el.type !== "checkbox" && el.type !== "hidden") {
      text = el.value || el.placeholder || "";
    }
    text = text.trim();
    if (!text || !visible(el)) {
      continue;
    }
    if (el.closest(":disabled") || el.closest("[aria-disabled='true']")) {
      continue;
    }
    checked += 1;
    const cs = getComputedStyle(el);
    const size = parseFloat(cs.fontSize);
    let fg = parse(el.tagName === "INPUT" && !el.value ? getComputedStyle(el, "::placeholder").color : cs.color);
    if (!fg) {
      continue;
    }
    fg = [fg[0], fg[1], fg[2], fg[3] * opacity(el)];
    let worst = Infinity;
    let worstBg = null;
    for (const g of grounds(el)) {
      const r = ratio(over(fg, g), g);
      if (r < worst) {
        worst = r;
        worstBg = g;
      }
    }
    if (size < 11 || worst < 4.5) {
      fails.push({
        where: describe(el),
        text: text.slice(0, 40),
        px: size,
        ratio: Math.round(worst * 100) / 100,
        color: cs.color,
        ground: worstBg ? "rgb(" + worstBg.slice(0, 3).map(Math.round).join(",") + ")" : "",
      });
    }
  }
  return { checked: checked, fails: fails };
})()
