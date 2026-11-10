/* Pocket Band screens. Everything is drawn into the 240x240 framebuffer and driven only by
 * controls the FM-1 really has: 7 encoders, 14 buttons, 27 keys. */
(function (root) {
'use strict';
const B = root.Brain, S = root.Sound, E = root.Engine, g = root.G;
const PAGE = { HOME: 'STAGE', GLO: 'MAP', EDIT: 'AUDITION', ENV: 'SOUND', LFO: 'EVOLVE', SEQ: 'GRID', FX: 'PERFORM', SCL: 'KEY', ARP: 'VISUALS', SAVE: 'CART' };
const UI = { cur: 'BOOT', sel: 2, oct: 0, booted: false, t0: performance.now(), toast: null, toastT: 0, pulse: new Float64Array(8), kick: -9, keyLit: new Float64Array(27), frame: 0 };
const T = () => (performance.now() - UI.t0) / 1000;
const song = () => E.song;
const tr = () => E.song.tracks[UI.sel];
const say = m => { UI.toast = m; UI.toastT = T() + 1.6; };
const pct = v => Math.round(v * 100);
const wrap = (v, n) => ((v % n) + n) % n;
const store = { get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } }, set(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* storage unavailable */ } } };
function keyName(s) { return B.NOTE_NAMES[s.root] + ' ' + s.scale.slice(0, 3); }
function genreName(s) { return s.blend ? B.GENRES[s.gA].name + ' X ' + B.GENRES[s.gB].name : B.GENRES[s.gA].name; }
function noteName(n) { return B.NOTE_NAMES[wrap(n, 12)] + (Math.floor(n / 12) - 1); }

/* ---------- shared drawing ---------- */
function topBar(title) {
  g.rect(0, 0, 240, 11, 3); g.text(3, 2, title, 0);
  const s = song(); if (!s) return;
  const pos = E.posNow();
  for (let i = 0; i < 4; i++) g.rect(206 + i * 6, 3, 4, 5, E.playing && pos >= 0 && ((pos >> 2) & 3) === i ? (i === 0 ? 5 : 0) : 2);
  g.textR(200, 2, s.bpm + ' ' + keyName(s), 0);
  if (E.rec && (UI.frame >> 4) & 1) g.rect(231, 3, 6, 5, 4); else if (E.playing) { g.rect(232, 3, 1, 5, 0); g.rect(233, 4, 1, 3, 0); g.rect(234, 5, 1, 1, 0); }
}
function knobBar(ks) {
  g.rect(0, 216, 240, 24, 1); g.rect(0, 216, 240, 1, 3);
  for (let i = 0; i < 7; i++) {
    const k = ks[i]; if (!k) continue;
    const cx = 1 + i * 34 + 17;
    g.textC(cx, 219, String(k[0]).slice(0, 5), 3); g.textC(cx, 229, String(k[1]).slice(0, 5), 7);
    if (i) g.rect(i * 34, 218, 1, 20, 2);
  }
}
function drawPat(pat, x, y, w, h, role, col) {
  g.rect(x, y, w, h, 1);
  if (role < 2 || role === 7) {
    const rh = h / 5;
    for (let s = 0; s < 64; s++) if (pat[s]) for (const e of pat[s]) g.rect(x + s * w / 64, y + h - (e.n + 1) * rh, Math.max(1, w / 64 - .4), Math.max(1, rh - 1), col);
    return;
  }
  let lo = 999, hi = -999;
  for (const st of pat) if (st) for (const e of st) { lo = Math.min(lo, e.n); hi = Math.max(hi, e.n); }
  if (lo > hi) return;
  const span = Math.max(6, hi - lo);
  for (let s = 0; s < 64; s++) if (pat[s]) for (const e of pat[s])
    g.rect(x + s * w / 64, y + h - 2 - (e.n - lo) / span * (h - 3), Math.max(1, Math.min(e.g, 8) * w / 64 - .6), 2, col);
}
const spec = { data: new Uint8Array(256), wave: new Uint8Array(512), peak: new Float32Array(40) };
function readAudio() { if (S.ready) { S.analyser.getByteFrequencyData(spec.data); S.analyser.getByteTimeDomainData(spec.wave); } }
function bars(x, y, w, h, n, seg) {
  const bw = Math.floor(w / n);
  for (let i = 0; i < n; i++) {
    const a = Math.floor(Math.pow(i / n, 1.7) * 150) + 1, b = Math.floor(Math.pow((i + 1) / n, 1.7) * 150) + 2;
    let v = 0; for (let k = a; k < b; k++) v = Math.max(v, spec.data[k]);
    v = S.ready ? v / 255 : .06 + .05 * Math.sin(T() * 2 + i * .7);
    const bh = Math.round(v * h);
    spec.peak[i] = Math.max(spec.peak[i] - h * .012, bh);
    for (let j = 0; j < bh; j += seg) g.rect(x + i * bw, y + h - j - (seg - 1), bw - 1, seg - 1 || 1, j > h * .78 ? 4 : j > h * .5 ? 5 : 6);
    g.rect(x + i * bw, y + h - Math.round(spec.peak[i]) - 1, bw - 1, 1, 7);
  }
}
function scope(x, y, w, h, col) {
  let py = y + h / 2;
  for (let i = 0; i < w; i++) {
    const v = S.ready ? (spec.wave[Math.floor(i * 380 / w)] - 128) / 128 : 0;
    const yy = Math.round(y + h / 2 - v * h * .9);
    g.line(x + i - 1, py, x + i, B.clamp(yy, y, y + h - 1), col); py = B.clamp(yy, y, y + h - 1);
  }
}
function bounce(i) { return Math.max(0, 3 - (S.now() - UI.pulse[i]) * 22); }
function member(i, x, y, scale, o) {
  const t = song().tracks[i], bo = t.mute ? 0 : bounce(i);
  g.sprite(i, x, y - Math.round(bo * scale / 2) + (t.mute ? scale : 0), scale, Object.assign({ sleep: t.mute, dim: t.mute, squash: bo > 1.5 }, o || {}));
  if (t.mute && scale > 1) g.text(x + 13 * scale, y - 2 - ((UI.frame >> 4) % 3) * 2, 'Z', 2);
}

/* ---------- world map ---------- */
const TOWNS = [[9, 3, 2], [11, 10, 3], [3, 17, 2], [4, 25, 3], [6, 5, 8], [10, 12, 8], [12, 19, 8], [13, 26, 9], [7, 3, 14], [0, 11, 14], [1, 18, 14], [5, 25, 15], [8, 7, 19], [2, 15, 19]];
const tiles = new Uint8Array(30 * 22);
(function buildMap() {
  const lat = (x, y) => B.hash(x * 31 + 7, y * 17 + 3);
  const sm = t => t * t * (3 - 2 * t);
  for (let y = 0; y < 22; y++) for (let x = 0; x < 30; x++) {
    const fx = x / 4.5, fy = y / 4.5, x0 = Math.floor(fx), y0 = Math.floor(fy), u = sm(fx - x0), v = sm(fy - y0);
    let h = (lat(x0, y0) * (1 - u) + lat(x0 + 1, y0) * u) * (1 - v) + (lat(x0, y0 + 1) * (1 - u) + lat(x0 + 1, y0 + 1) * u) * v;
    h += .25 * B.hash(x, y) - .1;
    let near = 99; for (const tw of TOWNS) near = Math.min(near, Math.abs(tw[1] - x) + Math.abs(tw[2] - y));
    tiles[y * 30 + x] = near <= 1 ? 2 : h < .34 ? 0 : h < .4 ? 1 : h < .68 ? 2 : h < .84 ? 3 : 4;
  }
})();
const map = { x: 92, y: 128, town: 9 };
function mapMix() {
  const d = TOWNS.map((tw, i) => ({ i, g: tw[0], d: Math.hypot(tw[1] * 8 + 4 - map.x, 12 + tw[2] * 8 + 4 - map.y) })).sort((a, b) => a.d - b.d);
  const blend = d[0].d < 14 ? 0 : Math.round(Math.min(.5, d[0].d / (d[0].d + d[1].d)) * 14);
  return { a: d[0], b: d[1], blend };
}
function travel(par) {
  if (!par) { const m = mapMix(); par = { gA: m.a.g, gB: m.blend ? m.b.g : m.a.g, blend: m.blend, seed: Math.floor(Math.random() * 0x7ffff) }; }
  const s = B.makeSong(par);
  E.load(s); if (!E.playing) E.start();
  go('STAGE'); say(genreName(s));
  return s;
}
E.onAuto = function () {       /* autopilot: wander to a neighbouring region for the next track */
  const a = Math.random() * 6.28, r = 30 + Math.random() * 50;
  map.x = B.clamp(map.x + Math.cos(a) * r, 8, 232); map.y = B.clamp(map.y + Math.sin(a) * r, 20, 180);
  const m = mapMix();
  say('NEXT STOP: ' + B.GENRES[m.a.g].name);
  return B.makeSong({ gA: m.a.g, gB: m.blend ? m.b.g : m.a.g, blend: m.blend, seed: Math.floor(Math.random() * 0x7ffff) });
};

/* ---------- screens ---------- */
const aud = { list: [], idx: 0, salt: 0 };
function audBuild() { aud.list = B.candidates(song(), UI.sel, aud.salt, E.energy); aud.idx = 0; E.preview = null; }
const grid = { step: 0, row: 0, base: 20, gate: 2 };
function gridFocus() {
  const t = tr(); grid.row = 0;
  if (t.role >= 2 && t.role !== 7) {
    const ds = []; for (const st of t.A) if (st) for (const e of st) ds.push(B.noteToDeg(song(), e.n));
    ds.sort((a, b) => a - b); grid.row = ds.length ? ds[ds.length >> 1] : 35; grid.base = grid.row - 6;
  }
}
const cart = { tab: 0, cur: 0, entry: 'AAAAAA', slot: 0, mode: 0 };
const vis = { mode: 0, stars: [] };
for (let i = 0; i < 70; i++) vis.stars.push({ x: Math.random() * 2 - 1, y: Math.random() * 2 - 1, z: Math.random() });
const PADS = ['DRM', 'PRC', 'BAS', 'CHD', 'PAD', 'LED', 'ARP', 'FX', 'INTR', 'GROV', 'BULD', 'DROP', 'BREK', 'OUTR', 'FILL', 'ST 8', 'ST16', 'ECHO', 'LCUT', 'SONG', 'AUTO', 'NEWB', 'RISE', 'BOOM', 'PEW', 'DICE', 'RST'];
const SECN = ['INTRO', 'GROOVE', 'BUILD', 'DROP', 'BREAK', 'OUTRO'];
const padDown = new Uint8Array(27);
let diceT = 0, bSalt = 0;

function selTrack(d) { UI.sel = wrap(UI.sel + d, 8); }
/* Re-roll dial: right makes a new idea for the selected part, left steps back through earlier ones. */
const ideas = { song: null, by: [] };
function ideaSlot() {
  if (ideas.song !== song()) { ideas.song = song(); ideas.by = []; }
  const t = tr();
  return ideas.by[UI.sel] || (ideas.by[UI.sel] = { list: [{ A: t.A, B: t.B, seed: t.seed }], idx: 0 });
}
function reroll(d) {
  const t = tr(), h = ideaSlot();
  h.list[h.idx] = { A: t.A, B: t.B, seed: t.seed };      /* keep any edits made to the current idea */
  if (d > 0 && h.idx === h.list.length - 1) {
    const seed = Math.floor(Math.random() * 0x7fffffff);
    h.list.push({ A: B.genPattern(song(), t.role, t.g, seed), B: B.genPattern(song(), t.role, t.g, seed + 7717), seed });
    if (h.list.length > 16) h.list.shift();
  }
  h.idx = B.clamp(h.idx + d, 0, h.list.length - 1);
  const it = h.list[h.idx]; t.A = it.A; t.B = it.B; t.seed = it.seed; t.mute = false;
  say(B.ROLES[t.role] + ' IDEA ' + (h.idx + 1) + '/' + h.list.length);
}
function tweak(obj, k, d, step, lo, hi) { obj[k] = B.clamp(Math.round((obj[k] + d * step) * 1000) / 1000, lo, hi); }
function blip() { if (!E.playing) E.live(UI.sel, 7, 0); }

const SCREENS = {
  BOOT: {
    draw() {
      const t = T(); g.cls(0);
      for (let i = 0; i < 40; i++) g.px((i * 53 + UI.frame) % 240, (i * 97) % 200, 1);
      g.textC(120, Math.min(44, -40 + t * 150), 'POCKET', 3, 4); g.textC(120, Math.min(80, -40 + (t - .25) * 190), 'BAND', 4, 4);
      if (t > 1.1) { g.textC(120, 118, 'FM-1 CUSTOM FIRMWARE', 2); g.textC(120, 128, 'BROWSER PROTOTYPE', 2); }
      for (let i = 0; i < 8; i++) {
        const x = Math.max(12 + i * 28, 250 - (t - 1.2 - i * .12) * 260);
        g.sprite(i, x, 150 - (((UI.frame >> 3) + i) & 1), 1, { squash: ((UI.frame >> 3) + i) & 1 });
      }
      if (t > 2 && (UI.frame >> 5) & 1) g.textC(120, 186, 'PRESS PLAY', 7, 2);
      g.textC(120, 226, 'SOUND STARTS WHEN YOU PRESS PLAY', 2);
    }
  },
  STAGE: {
    title: () => 'STAGE',
    knobs() { const t = tr(); return [['TRACK', B.ROLES[t.role]], ['VOL', pct(t.vol)], ['ENRGY', pct(E.energy)], ['MORPH', pct(E.morph)], ['FILTR', E.filter === 0 ? 'OFF' : (E.filter < 0 ? 'LP' : 'HP') + pct(Math.abs(E.filter))], ['TEMPO', song().bpm], ['REROL', '#' + (ideaSlot().idx + 1)]]; },
    turn(k, d) {
      const t = tr();
      if (k === 0) selTrack(d); else if (k === 1) { tweak(t, 'vol', d, .05, 0, 1); E.applyMix(); }
      else if (k === 2) tweak(E, 'energy', d, .05, 0, 1); else if (k === 3) tweak(E, 'morph', d, .05, 0, 1);
      else if (k === 4) { tweak(E, 'filter', d, .05, -1, 1); E.setFilter(E.filter); }
      else if (k === 5) { tweak(song(), 'bpm', d, 1, 40, 200); S.setBpm(song().bpm); }
      else reroll(d > 0 ? 1 : -1);
    },
    press() { const t = tr(); t.mute = !t.mute; say(B.ROLES[t.role] + (t.mute ? ' SLEEPS' : ' WAKES UP')); },
    draw() {
      const s = song(), m = genreName(s) + '  *  ' + s.bpm + ' BPM  *  ' + B.NOTE_NAMES[s.root] + ' ' + s.scale + '  *  PASSWORD ' + B.encode(s) + '  *  SECTION ' + E.section + '  *  ';
      const off = Math.floor(T() * 40) % (m.length * 6);
      g.text(-off, 14, m + m + m, 3);
      g.rect(0, 23, 240, 1, 2); bars(0, 25, 240, 32, 30, 2); g.rect(0, 58, 240, 1, 2);
      for (let i = 0; i < 8; i++) {
        const x = (i % 4) * 60, y = 60 + (i >> 2) * 73, t = s.tracks[i];
        for (let k = 0; k < 15; k++) g.rect(x + k * 4, y + 43 + (k & 1) * 2, 4, 2, 1);
        member(i, x + 14, y + 8, 2);
        g.textC(x + 30, y + 49, B.ROLES[t.role], t.mute ? 2 : 3);
        g.bar(x + 10, y + 59, 40, 3, t.vol, t.mute ? 2 : g.BODY[i]);
        if (i === UI.sel) { g.frame(x + 1, y + 1, 58, 68, 7); const a = (UI.frame >> 3) & 1; g.rect(x + 27, y + 2 + a, 6, 1, 7); g.rect(x + 28, y + 3 + a, 4, 1, 7); g.rect(x + 29, y + 4 + a, 2, 1, 7); }
      }
      g.text(3, 207, E.songMode ? 'SONG: ' + E.section + (E.auto ? '  AUTOPILOT' : '') : 'HOME: WAKE/SLEEP  K7: NEW PART IDEA', 2);
    }
  },
  MAP: {
    title: () => 'WORLD MAP',
    knobs() { return [['EAST', Math.round(map.x)], ['SOUTH', Math.round(map.y)], ['TOWN', B.GENRES[TOWNS[map.town][0]].tag]]; },
    turn(k, d) {
      if (k === 0) map.x = B.clamp(map.x + d * 4, 6, 234); else if (k === 1) map.y = B.clamp(map.y + d * 4, 18, 184);
      else if (k === 2) { map.town = wrap(map.town + d, TOWNS.length); map.x = TOWNS[map.town][1] * 8 + 4; map.y = 12 + TOWNS[map.town][2] * 8 + 4; }
    },
    press() { travel(); },
    draw() {
      const f = UI.frame >> 4;
      for (let y = 0; y < 22; y++) for (let x = 0; x < 30; x++) {
        const tt = tiles[y * 30 + x], px = x * 8, py = 12 + y * 8;
        if (tt === 0) { g.rect(px, py, 8, 8, 1); const o = (f + x + y * 3) & 7; g.rect(px + o, py + 2, 2, 1, 2); g.rect(px + ((o + 4) & 7), py + 6, 2, 1, 2); }
        else { g.rect(px, py, 8, 8, 0);
          if (tt === 1) { g.px(px + 2, py + 3, 1); g.px(px + 6, py + 5, 1); g.px(px + 4, py + 1, 1); }
          else if (tt === 2) { if ((x * 7 + y * 3) % 3 === 0) { g.px(px + 3, py + 5, 1); g.px(px + 4, py + 4, 1); } }
          else if (tt === 3) { g.rect(px + 3, py + 1, 2, 1, 2); g.rect(px + 2, py + 2, 4, 2, 2); g.rect(px + 1, py + 4, 6, 2, 2); g.rect(px + 3, py + 6, 2, 2, 1); }
          else { g.line(px, py + 7, px + 4, py + 1, 3); g.line(px + 4, py + 1, px + 7, py + 7, 3); g.rect(px + 3, py + 4, 2, 3, 2); g.px(px + 4, py + 2, 7); }
        }
      }
      const m = mapMix();
      TOWNS.forEach((tw, i) => {
        const cx = tw[1] * 8, cy = 12 + tw[2] * 8, hot = i === m.a.i || (m.blend && i === m.b.i);
        g.rect(cx, cy + 3, 8, 5, hot && (UI.frame >> 3) & 1 ? 7 : 5); g.rect(cx + 1, cy + 1, 6, 2, 4); g.rect(cx + 3, cy, 2, 1, 4); g.rect(cx + 3, cy + 5, 2, 3, 3);
        g.textC(cx + 4, cy + 10, B.GENRES[tw[0]].tag, 3, 1, 0);
      });
      g.line(map.x, map.y, TOWNS[m.a.i][1] * 8 + 4, 12 + TOWNS[m.a.i][2] * 8 + 4, 7, true);
      if (m.blend) g.line(map.x, map.y, TOWNS[m.b.i][1] * 8 + 4, 12 + TOWNS[m.b.i][2] * 8 + 4, 4, true);
      g.sprite(5, map.x - 8, map.y - 10 - ((UI.frame >> 3) & 1), 1, { squash: (UI.frame >> 3) & 1 });
      g.rect(0, 188, 240, 28, 0); g.rect(0, 188, 240, 1, 3);
      const pa = 100 - Math.round(m.blend / 14 * 100), A = B.GENRES[m.a.g], Bg = B.GENRES[m.b.g];
      g.text(3, 192, m.blend ? A.name + ' ' + pa + '% + ' + Bg.name + ' ' + (100 - pa) + '%' : A.name + ' (PURE)', 3);
      g.text(3, 203, A.bpm[0] + '-' + A.bpm[1] + ' BPM', 2); g.textR(237, 203, 'PRESS GLO TO TRAVEL', (UI.frame >> 4) & 1 ? 7 : 2);
    }
  },
  AUDITION: {
    title: () => 'AUDITION: ' + B.ROLES[tr().role],
    enter() { audBuild(); },
    leave() { E.preview = null; },
    knobs() { return [['TRACK', B.ROLES[tr().role]], ['PICK', aud.idx ? '#' + aud.idx : 'NOW'], ['MORE', 'BATCH']]; },
    turn(k, d) {
      if (k === 0) { selTrack(d); audBuild(); }
      else if (k === 1) { aud.idx = B.clamp(aud.idx + d, 0, 3); const c = aud.list[aud.idx - 1]; E.preview = c ? { ti: UI.sel, pat: c.pat, snd: c.snd } : null; }
      else if (k === 2) { aud.salt++; audBuild(); say('NEW BATCH'); }
    },
    press() {
      const c = aud.list[aud.idx - 1], t = tr(); if (!c) { say('TURN PICK FIRST'); return; }
      t.A = c.pat; t.g = c.g; t.seed = c.seed; t.snd = c.snd; t.B = B.genPattern(song(), t.role, c.g, c.seed + 7717); t.mute = false;
      say('HIRED!'); audBuild();
    },
    draw() {
      const t = tr();
      const card = (y, h, i, name, snd, fit, pat, body) => {
        const hot = aud.idx === i; g.rect(1, y, 238, h, hot ? 1 : 0); g.frame(1, y, 238, h, hot ? 7 : 2);
        if (h > 40) g.sprite(UI.sel, 5, y + 11 - (hot ? (UI.frame >> 3) & 1 : 0), 2, { body }); else g.sprite(UI.sel, 6, y + 7, 1);
        const tx = h > 40 ? 42 : 28;
        g.text(tx, y + 5, name, 3); g.text(tx, y + 16, 'SOUND ' + snd, 2);
        if (fit) { g.text(tx, y + 30, 'FIT', 3); g.bar(tx + 22, y + 31, 44, 5, (fit - 40) / 60, fit > 85 ? 6 : 5, 0); g.text(tx + 70, y + 30, fit, 7); }
        drawPat(pat, 134, y + 5, 100, h - 10, t.role, hot ? 7 : 3);
        if (hot) g.text(tx, y + h - 11, h > 40 ? 'EDIT: HIRE' : '', 4);
      };
      card(14, 30, 0, 'NOW: ' + B.GENRES[t.g].name, t.snd, 0, t.A, undefined);
      aud.list.forEach((c, i) => card(46 + i * 56, 54, i + 1, B.GENRES[c.g].name + ' ' + B.ROLES[t.role], c.snd, c.fit, c.pat, [4, 5, 6][i]));
    }
  },
  SOUND: {
    title: () => 'SOUND: ' + B.ROLES[tr().role],
    knobs() { const t = tr(), m = t.mac; return [['TRACK', B.ROLES[t.role]], ['SOUND', t.snd], ['BRGHT', pct(m.bright)], ['PUNCH', pct(m.punch)], ['WEIRD', pct(m.weird)], ['SPACE', pct(m.space)], ['VOL', pct(t.vol)]]; },
    turn(k, d) {
      const t = tr(), m = t.mac, list = B.SOUNDS[t.role];
      if (k === 0) selTrack(d);
      else if (k === 1) { t.snd = list[wrap(list.indexOf(t.snd) + d, list.length)]; blip(); }
      else if (k === 2) tweak(m, 'bright', d, .05, 0, 1); else if (k === 3) tweak(m, 'punch', d, .05, 0, 1);
      else if (k === 4) tweak(m, 'weird', d, .05, 0, 1); else if (k === 5) { tweak(m, 'space', d, .05, 0, 1); E.applyMix(); }
      else { tweak(t, 'vol', d, .05, 0, 1); E.applyMix(); }
    },
    press() {
      const t = tr(), list = B.SOUNDS[t.role], r = Math.random;
      t.snd = list[Math.floor(r() * list.length)];
      t.mac.bright = .25 + r() * .6; t.mac.punch = .2 + r() * .7; t.mac.weird = r() < .5 ? 0 : r() * .6; t.mac.space = t.role < 3 ? r() * .15 : r() * .7;
      E.applyMix(); diceT = T() + .5; blip();
    },
    draw() {
      const t = tr(), m = t.mac, roll = T() < diceT;
      member(UI.sel, 10, 26, 4);
      g.text(86, 20, B.ROLES[t.role], 3, 2); g.text(86, 42, 'SOUND', 2); g.text(122, 42, roll ? '??????' : t.snd, 7);
      g.text(86, 54, 'FROM', 2); g.text(116, 54, B.GENRES[t.g].name, 3);
      g.text(86, 70, roll ? 'ROLL!' : 'ENV: ROLL THE DICE', roll ? 4 : 2);
      [['BRIGHT', m.bright, 5], ['PUNCH', m.punch, 4], ['WEIRD', m.weird, 6], ['SPACE', m.space, 5]].forEach((r, i) => {
        const y = 100 + i * 20, v = roll ? Math.random() : r[1];
        g.text(8, y + 2, r[0], 3);
        for (let k = 0; k < 20; k++) g.rect(54 + k * 8, y, 6, 11, k < Math.round(v * 20) ? r[2] : 1);
        g.textR(236, y + 2, pct(v), 7);
      });
      g.frame(4, 182, 232, 30, 2); scope(6, 184, 228, 26, 7);
    }
  },
  EVOLVE: {
    title: () => 'EVOLVE + MORPH',
    knobs() { return [['MORPH', pct(E.morph)], ['EVOLV', E.evolve.rate ? E.evolve.rate + 'BAR' : 'OFF'], ['ENRGY', pct(E.energy)]]; },
    turn(k, d) {
      if (k === 0) tweak(E, 'morph', d, .05, 0, 1);
      else if (k === 1) { const r = [0, 8, 4, 2]; E.evolve.rate = r[B.clamp(r.indexOf(E.evolve.rate) + d, 0, 3)]; }
      else if (k === 2) tweak(E, 'energy', d, .05, 0, 1);
    },
    press() { B.newB(song(), ++bSalt); say('NEW B VERSIONS'); },
    oct(d) { if (E.evolve.last) { E.judge(d > 0); say(d > 0 ? 'KEPT - NOTED' : 'KILLED - NOTED'); } else say('NOTHING TO JUDGE'); },
    draw() {
      g.text(4, 16, 'A', 3, 2); g.textR(236, 16, 'B', 4, 2);
      g.rect(24, 21, 192, 5, 1); g.rect(24, 21, Math.round(192 * E.morph), 5, 4); g.rect(22 + Math.round(192 * E.morph), 17, 5, 13, 7);
      const pos = E.posNow(), thr = 1 - E.energy;
      for (let i = 0; i < 8; i++) {
        const t = song().tracks[i], y = 36 + i * 13;
        g.text(2, y + 2, B.ROLES[t.role].slice(0, 3), t.mute ? 2 : 3); g.rect(24, y, 192, 11, 1);
        for (let s = 0; s < 64; s++) {
          const fromB = E.morph > 0 && B.hash(i * 64 + s, 7) < E.morph, st = (fromB ? t.B : t.A)[s];
          if (st && st.some(e => e.p >= thr)) g.rect(24 + s * 3, y + 1, 2, 9, t.mute ? 2 : fromB ? 4 : 3);
        }
        if (pos >= 0) g.rect(24 + pos * 3, y, 1, 11, 7);
      }
      const ev = E.evolve, l = ev.last;
      g.text(3, 144, ev.rate ? 'EVOLVING EVERY ' + ev.rate + ' BARS' : 'EVOLVE IS OFF - TURN K2', ev.rate ? 6 : 2);
      if (l) { g.rect(0, 155, 240, 22, 1); g.text(3, 158, 'NEW: ' + B.ROLES[song().tracks[l.ti].role] + ' ' + l.op, 7); g.text(3, 168, 'OCT- KILL IT   OCT+ KEEP IT', (UI.frame >> 4) & 1 ? 4 : 3); }
      else g.text(3, 160, 'LFO: FRESH B VERSIONS', 2);
      g.text(3, 184, 'YOUR TASTE', 2);
      B.OPS.forEach((o, i) => { g.text(3 + i * 48, 195, o, 3); g.bar(3 + i * 48, 205, 40, 4, ev.taste[o] / 4, 5); });
    }
  },
  GRID: {
    title: () => 'GRID: ' + B.ROLES[tr().role],
    enter() { gridFocus(); },
    knobs() { const t = tr(), d = t.role < 2 || t.role === 7; return [['TRACK', B.ROLES[t.role]], ['STEP', grid.step + 1], [d ? 'PAD' : 'NOTE', d ? grid.row + 1 : noteName(B.sn(song(), grid.row))], ['GATE', grid.gate], ['BAR', (grid.step >> 4) + 1]]; },
    turn(k, d) {
      const drum = tr().role < 2 || tr().role === 7;
      if (k === 0) { selTrack(d); gridFocus(); }
      else if (k === 1) grid.step = wrap(grid.step + d, 64);
      else if (k === 2) { if (drum) grid.row = B.clamp(grid.row + d, 0, 4); else { grid.row = B.clamp(grid.row + d, 14, 63); if (grid.row < grid.base) grid.base = grid.row; if (grid.row > grid.base + 13) grid.base = grid.row - 13; } }
      else if (k === 3) grid.gate = B.clamp(grid.gate + d, 1, 16);
      else if (k === 4) grid.step = wrap(grid.step + d * 16, 64);
    },
    press() { this.put(tr().role < 2 || tr().role === 7 ? grid.row : B.sn(song(), grid.row), true); },
    put(n, toggle) {
      const t = tr(), A = t.A, s = grid.step, drum = t.role < 2 || t.role === 7, st = A[s] || [];
      const has = st.findIndex(e => e.n === n);
      if (has >= 0 && toggle) st.splice(has, 1);
      else if (has < 0) { if (!drum && !toggle) st.length = 0; st.push({ n, v: 100, g: drum ? 1 : grid.gate, p: 1 }); }
      A[s] = st.length ? st : null;
    },
    draw() {
      const t = tr(), s = song(), drum = t.role < 2 || t.role === 7, page = grid.step >> 4, rows = drum ? 5 : 14, rh = drum ? 36 : 13, y0 = 26, pos = E.posNow();
      g.text(3, 14, 'BAR ' + (page + 1) + '/4', 3); g.textR(237, 14, 'SEQ: PUT / REMOVE', 2);
      for (let r = 0; r < rows; r++) {
        const y = y0 + (rows - 1 - r) * rh, deg = grid.base + r, isRoot = !drum && wrap(deg, 7) === 0;
        g.text(1, y + (rh >> 1) - 3, drum ? (t.role === 7 ? ['BOOM', 'RISE', 'BLIP', '-', '-'][r] : B.PAD_LABELS[t.role][r]) : noteName(B.sn(s, deg)), isRoot ? 5 : (drum ? r : deg) === grid.row ? 7 : 2);
        for (let c = 0; c < 16; c++) g.rect(32 + c * 13, y, 12, rh - 1, (c >> 2) & 1 ? 0 : 1);
      }
      for (let c = 0; c < 16; c++) {
        const st = t.A[page * 16 + c]; if (!st) continue;
        for (const e of st) {
          const r = drum ? e.n : B.noteToDeg(s, e.n) - grid.base; if (r < 0 || r >= rows) continue;
          const y = y0 + (rows - 1 - r) * rh, w = drum ? 12 : Math.min(16 - c, e.g) * 13 - 1;
          g.rect(32 + c * 13, y, w, rh - 1, e.p >= 1 - E.energy ? g.BODY[UI.sel] : 2); g.rect(32 + c * 13, y, 2, rh - 1, 3);
        }
      }
      if (pos >= 0 && (pos >> 4) === page) g.rect(32 + (pos & 15) * 13, 23, 12, 2, 7);
      const cr = drum ? grid.row : grid.row - grid.base, cy = y0 + (rows - 1 - cr) * rh;
      g.frame(31 + (grid.step & 15) * 13, cy - 1, 14, rh + 1, (UI.frame >> 3) & 1 ? 7 : 4);
    }
  },
  PERFORM: {
    title: () => 'PERFORM',
    knobs() { const s = song(); return [['FILTR', E.filter === 0 ? 'OFF' : (E.filter < 0 ? 'LP' : 'HP') + pct(Math.abs(E.filter))], ['ENRGY', pct(E.energy)], ['MORPH', pct(E.morph)], ['TEMPO', s.bpm], ['SWING', pct(s.swing)]]; },
    turn(k, d) {
      const s = song();
      if (k === 0) { tweak(E, 'filter', d, .05, -1, 1); E.setFilter(E.filter); } else if (k === 1) tweak(E, 'energy', d, .05, 0, 1);
      else if (k === 2) tweak(E, 'morph', d, .05, 0, 1); else if (k === 3) { tweak(s, 'bpm', d, 1, 40, 200); S.setBpm(s.bpm); }
      else if (k === 4) tweak(s, 'swing', d, .01, 0, .33);
    },
    press() { say('KEYS ARE THE PADS HERE'); },
    key(i, down) {
      padDown[i] = down ? 1 : 0; const s = song();
      if (i === 14) E.setHeld('fill', down); else if (i === 15) E.setHeld('stut', down ? 2 : 0); else if (i === 16) E.setHeld('stut', down ? 1 : 0);
      else if (i === 17) E.setHeld('echo', down); else if (i === 18) E.setHeld('lowcut', down);
      if (!down) return;
      if (i < 8) s.tracks[i].mute = !s.tracks[i].mute;
      else if (i < 14) { E.queued = SECN[i - 8]; if (!E.playing) E.start(); }
      else if (i === 19) { E.songMode = !E.songMode; if (E.songMode) { E.chainIdx = -1; E.chainLeft = 0; if (!E.playing) E.start(); } else E.auto = false; say(E.songMode ? 'SONG MODE ON' : 'SONG MODE OFF'); }
      else if (i === 20) { E.auto = !E.auto; if (E.auto && !E.songMode) { E.songMode = true; E.chainIdx = -1; E.chainLeft = 0; if (!E.playing) E.start(); } say(E.auto ? 'AUTOPILOT ON' : 'AUTOPILOT OFF'); }
      else if (i === 21) { B.newB(s, ++bSalt); say('NEW B VERSIONS'); }
      else if (i === 22) E.oneShot('riser'); else if (i === 23) E.oneShot('impact'); else if (i === 24) E.oneShot('pew');
      else if (i === 25) { const n = B.makeSong({ gA: s.gA, gB: s.gB, blend: s.blend, seed: Math.floor(Math.random() * 0x7ffff) }); E.load(n); say('NEW SKETCH NEXT BAR'); }
      else if (i === 26) { E.setFilter(0); E.energy = .6; E.morph = 0; E.songMode = E.auto = false; E.queued = null; say('RESET'); }
    },
    draw() {
      const s = song(), k = Math.max(0, 1 - (S.now() - UI.kick) * 5);
      g.textC(120, 15, E.section, k > .5 ? 7 : 3, 3);
      if (E.queued && (UI.frame >> 3) & 1) g.textC(120, 40, 'NEXT BAR: ' + E.queued, 4);
      else if (E.songMode) g.textC(120, 40, 'SONG' + (E.auto ? ' + AUTOPILOT' : '') + '  ' + Math.max(0, E.chainLeft) + ' BARS LEFT', 6);
      else g.textC(120, 40, 'PRESS A SECTION PAD', 2);
      E.CHAIN.forEach((c, i) => { const hot = E.songMode && i === E.chainIdx; g.rect(4 + i * 29, 51, 27, 9, hot ? 7 : 1); g.textC(4 + i * 29 + 14, 52, c[0].slice(0, 4), hot ? 0 : 2); });
      for (let i = 0; i < 8; i++) member(i, 8 + i * 29, 66, 1);
      for (let i = 0; i < 27; i++) {
        const x = 3 + (i % 9) * 26, y = 90 + Math.floor(i / 9) * 24;
        let lit = padDown[i];
        if (i < 8) lit = !s.tracks[i].mute; else if (i < 14) lit = E.section === SECN[i - 8] || (E.queued === SECN[i - 8] && (UI.frame >> 2) & 1);
        else if (i === 19) lit = E.songMode; else if (i === 20) lit = E.auto;
        const col = i < 8 ? 6 : i < 14 ? 5 : i < 19 ? 4 : 2;
        g.rect(x, y, 25, 22, lit ? col : 1); g.frame(x, y, 25, 22, lit ? 7 : col);
        g.textC(x + 13, y + 4, PADS[i], lit ? 0 : 3); g.textC(x + 13, y + 13, i + 1, lit ? 0 : 2);
      }
      g.frame(4, 166, 232, 46, 2); scope(6, 168, 228, 42, k > .4 ? 7 : 6);
    }
  },
  KEY: {
    title: () => 'KEY + CHORDS',
    knobs() { const s = song(); return [['ROOT', B.NOTE_NAMES[s.root]], ['SCALE', s.scale], ['TEMPO', s.bpm], ['SWING', pct(s.swing)]]; },
    turn(k, d) {
      const s = song();
      if (k === 0) B.retune(s, s.root + d, s.scale);
      else if (k === 1) B.retune(s, s.root, B.SCALE_NAMES[wrap(B.SCALE_NAMES.indexOf(s.scale) + d, B.SCALE_NAMES.length)]);
      else if (k === 2) { tweak(s, 'bpm', d, 1, 40, 200); S.setBpm(s.bpm); } else if (k === 3) tweak(s, 'swing', d, .01, 0, .33);
    },
    press() { B.reharm(song()); say('NEW CHORDS ' + (song().progIdx + 1) + '/' + song().progs.length); },
    draw() {
      const s = song(), sc = B.SCALES[s.scale], inS = n => sc.indexOf(wrap(n - s.root, 12)) >= 0, pos = E.posNow();
      g.textC(120, 15, B.NOTE_NAMES[s.root] + ' ' + s.scale, 3, 2);
      const wh = [0, 2, 4, 5, 7, 9, 11], bl = [[1, 0], [3, 1], [6, 3], [8, 4], [10, 5]];
      wh.forEach((n, i) => { const x = 15 + i * 30; g.rect(x, 36, 29, 60, inS(n) ? 7 : 2); g.frame(x, 36, 29, 60, 3); if (inS(n)) g.rect(x + 11, 82, 7, 7, n === s.root ? 4 : 6); g.textC(x + 15, 72, B.NOTE_NAMES[n], inS(n) ? 0 : 1); });
      bl.forEach(b => { const x = 15 + b[1] * 30 + 20; g.rect(x, 36, 19, 36, 3); if (inS(b[0])) g.rect(x + 6, 58, 7, 7, b[0] === s.root ? 4 : 6); });
      g.text(15, 102, 'LIT NOTES ARE IN THE SCALE. KEYS SNAP', 2); g.text(15, 111, 'TO THEM, SO NOTHING SOUNDS WRONG.', 2);
      s.prog.forEach((c, i) => {
        const hot = pos >= 0 && (pos >> 4) === i, x = 6 + i * 58;
        g.rect(x, 126, 54, 44, hot ? 5 : 1); g.frame(x, 126, 54, 44, hot ? 7 : 2);
        g.text(x + 4, 130, 'BAR ' + (i + 1), hot ? 0 : 2); g.textC(x + 27, 146, B.chordName(s, c), hot ? 0 : 3, 2);
      });
      g.text(6, 178, 'SCL: TRY A NEW CHORD SET (' + (s.progIdx + 1) + '/' + s.progs.length + ')', 3);
      g.text(6, 192, 'TEMPO ' + s.bpm + ' BPM   SWING ' + pct(s.swing) + '%', 2);
      g.text(6, 203, 'CHORDS ' + (s.sev ? 'WITH 7THS' : 'PLAIN TRIADS'), 2);
    }
  },
  VISUALS: {
    title: () => 'VISUALS: ' + ['BARS', 'SCOPE', 'STARS', 'ROLL'][vis.mode],
    knobs() { return [['MODE', ['BARS', 'SCOPE', 'STARS', 'ROLL'][vis.mode]], ['ENRGY', pct(E.energy)], ['FILTR', E.filter === 0 ? 'OFF' : pct(E.filter)]]; },
    turn(k, d) { if (k === 0) vis.mode = wrap(vis.mode + d, 4); else if (k === 1) tweak(E, 'energy', d, .05, 0, 1); else if (k === 2) { tweak(E, 'filter', d, .05, -1, 1); E.setFilter(E.filter); } },
    press() { vis.mode = wrap(vis.mode + 1, 4); },
    draw() {
      const k = Math.max(0, 1 - (S.now() - UI.kick) * 4);
      if (vis.mode === 0) { bars(0, 16, 240, 170, 30, 4); for (let i = 0; i < 8; i++) member(i, 8 + i * 29, 194, 1); }
      else if (vis.mode === 1) { for (let y = 20; y < 210; y += 19) g.rect(0, y, 240, 1, 1); for (let x = 0; x < 240; x += 24) g.rect(x, 14, 1, 200, 1); scope(0, 30, 240, 170, 6); scope(0, 31, 240, 170, 7); }
      else if (vis.mode === 2) {
        for (const st of vis.stars) {
          st.z -= .006 + k * .03; if (st.z <= .02) { st.z = 1; st.x = Math.random() * 2 - 1; st.y = Math.random() * 2 - 1; }
          const x = 120 + st.x / st.z * 90, y = 112 + st.y / st.z * 80, sz = st.z < .3 ? 3 : st.z < .6 ? 2 : 1;
          if (y > 12 && y < 214) g.rect(x, y, sz, sz, st.z < .3 ? 7 : st.z < .6 ? 3 : 2);
        }
        const r = 14 + k * 16; for (let a = 0; a < 40; a++) g.px(120 + Math.cos(a * .157) * r, 112 + Math.sin(a * .157) * r, k > .3 ? 4 : 5);
        for (let i = 0; i < 8; i++) member(i, 8 + i * 29, 194, 1);
      } else {
        const now = S.now();
        for (let i = 0; i < 8; i++) { const y = 14 + i * 25; g.rect(0, y + 24, 240, 1, 1); g.text(2, y + 9, B.ROLES[song().tracks[i].role].slice(0, 3), 2); }
        g.rect(200, 12, 1, 203, 7);
        for (const e of E.vis) {
          if (e.kick) continue;
          const x = 200 - (now - e.t) * 70, w = Math.max(2, e.g * 70), t = song().tracks[e.ti], drum = t.role < 2 || t.role === 7;
          if (x + w < 22 || x > 240) continue;
          const yy = 14 + e.ti * 25 + (drum ? 20 - e.n * 4 : 21 - wrap(e.n, 24) * 20 / 24);
          g.rect(Math.max(22, x), yy, w, 3, x <= 200 && x + w >= 200 ? 7 : g.BODY[e.ti]);
        }
      }
    }
  },
  CART: {
    title: () => 'CARTRIDGE',
    enter() { cart.entry = B.encode(song()); },
    knobs() {
      const tab = ['CODE', 'SLOTS', 'SKIN'][cart.tab];
      if (cart.tab === 0) return [['PAGE', tab], ['SPOT', cart.cur + 1], ['LETTR', cart.entry[cart.cur]]];
      if (cart.tab === 1) return [['PAGE', tab], ['SLOT', cart.slot + 1], ['DO', cart.mode ? 'LOAD' : 'SAVE']];
      return [['PAGE', tab], ['SKIN', g.skin() + 1]];
    },
    turn(k, d) {
      if (k === 0) cart.tab = wrap(cart.tab + d, 3);
      else if (cart.tab === 0 && k === 1) cart.cur = wrap(cart.cur + d, 6);
      else if (cart.tab === 0 && k === 2) { const i = wrap(B.ALPHA.indexOf(cart.entry[cart.cur]) + d, 32); cart.entry = cart.entry.slice(0, cart.cur) + B.ALPHA[i] + cart.entry.slice(cart.cur + 1); }
      else if (cart.tab === 1 && k === 1) cart.slot = wrap(cart.slot + d, 4);
      else if (cart.tab === 1 && k === 2) cart.mode = wrap(cart.mode + d, 2);
      else if (cart.tab === 2 && k === 1) { g.setSkin(g.skin() + d); store.set('pb_skin', g.skin()); }
    },
    press() {
      if (cart.tab === 0) { const p = B.decode(cart.entry); if (p) travel(p); else say('NO SUCH PLACE'); }
      else if (cart.tab === 1) {
        if (!cart.mode) { store.set('pb_slot' + cart.slot, JSON.stringify(song())); say('SAVED TO SLOT ' + (cart.slot + 1)); }
        else { const j = store.get('pb_slot' + cart.slot); if (j) { try { E.load(JSON.parse(j)); say('LOADED SLOT ' + (cart.slot + 1)); } catch (e) { say('SLOT IS DAMAGED'); } } else say('SLOT IS EMPTY'); }
      } else say(g.SKINS[g.skin()].name);
    },
    draw() {
      ['PASSWORD', 'SAVE SLOTS', 'SKINS'].forEach((n, i) => { g.rect(2 + i * 79, 14, 77, 11, i === cart.tab ? 3 : 1); g.textC(2 + i * 79 + 39, 16, n, i === cart.tab ? 0 : 2); });
      if (cart.tab === 0) {
        g.text(8, 34, 'THIS SKETCH', 2); g.textC(120, 46, B.encode(song()), 7, 4);
        g.text(8, 92, 'ENTER A PASSWORD', 2);
        for (let i = 0; i < 6; i++) { const x = 30 + i * 30; g.rect(x, 104, 26, 30, i === cart.cur ? 5 : 1); g.textC(x + 13, 109, cart.entry[i], i === cart.cur ? 0 : 3, 3); }
        g.text(8, 146, 'SAVE: TRAVEL TO THIS PASSWORD', (UI.frame >> 4) & 1 ? 7 : 3);
        g.text(8, 166, 'A PASSWORD REBUILDS THE SKETCH AS IT', 2); g.text(8, 175, 'WAS GENERATED: PLACE, KEY, TEMPO AND', 2); g.text(8, 184, 'EVERY PART. EDITS MADE AFTERWARDS', 2); g.text(8, 193, 'LIVE IN THE SAVE SLOTS INSTEAD.', 2);
      } else if (cart.tab === 1) {
        for (let i = 0; i < 4; i++) {
          const y = 34 + i * 36, hot = i === cart.slot; let label = 'EMPTY';
          const j = store.get('pb_slot' + i); if (j) { try { const s = JSON.parse(j); label = genreName(s).slice(0, 22) + ' ' + s.bpm; } catch (e) { label = 'DAMAGED'; } }
          g.rect(4, y, 232, 32, hot ? 1 : 0); g.frame(4, y, 232, 32, hot ? 7 : 2);
          g.text(10, y + 6, 'SLOT ' + (i + 1), hot ? 7 : 2); g.text(10, y + 18, label, 3);
          if (hot) g.textR(230, y + 6, 'SAVE: ' + (cart.mode ? 'LOAD' : 'SAVE HERE'), 4);
        }
        g.text(6, 184, 'SLOTS KEEP EVERY EDIT. IN THIS', 2); g.text(6, 193, 'PROTOTYPE THEY LIVE IN YOUR BROWSER.', 2);
      } else {
        g.SKINS.forEach((sk, i) => {
          const y = 30 + i * 20, hot = i === g.skin();
          g.text(8, y + 5, sk.name, hot ? 7 : 3); if (hot) g.frame(2, y, 236, 18, 7);
          if (hot) for (let c = 0; c < 8; c++) { g.rect(100 + c * 16, y + 3, 14, 12, c); g.frame(100 + c * 16, y + 3, 14, 12, 2); }
        });
      }
    }
  }
};

function go(name) {
  const old = SCREENS[UI.cur]; if (old && old.leave) old.leave();
  UI.cur = name; const s = SCREENS[name]; if (s.enter) s.enter();
}
function boot() {
  if (UI.booted) return;
  S.init(); S.resume(); UI.booted = true;
  S.fx(7, 'ding', S.now() + .02, .5);
  map.x = 11 * 8 + 4; map.y = 12 + 14 * 8 + 4;
  travel({ gA: 0, gB: 0, blend: 0, seed: Math.floor(Math.random() * 0x7ffff) });
}
UI.button = function (name) {
  if (!UI.booted) { boot(); return; }
  S.resume();
  const scr = SCREENS[UI.cur];
  if (name === 'PLAY') E.toggle();
  else if (name === 'REC') { E.rec = !E.rec; say(E.rec ? 'REC ON - PLAY THE KEYS' : 'REC OFF'); }
  else if (name === 'OCT-' || name === 'OCT+') { const d = name === 'OCT+' ? 1 : -1; if (scr.oct) scr.oct(d); else { UI.oct = B.clamp(UI.oct + d, -2, 2); say('OCTAVE ' + (UI.oct > 0 ? '+' : '') + UI.oct); } }
  else if (PAGE[name]) { if (PAGE[name] === UI.cur) scr.press(); else go(PAGE[name]); }
};
UI.turn = function (k, d) { if (!UI.booted || !d) return; const scr = SCREENS[UI.cur]; if (scr.turn) scr.turn(k, d); };
UI.key = function (i, down) {
  if (!UI.booted) { if (down) boot(); return; }
  const scr = SCREENS[UI.cur];
  if (scr.key) { scr.key(i, down); return; }
  if (!down) return;
  const n = E.live(UI.sel, i, UI.oct);
  UI.keyLit[i] = performance.now() + 140;
  if (UI.cur === 'GRID' && !E.playing && n >= 0) { SCREENS.GRID.put(n, false); grid.step = wrap(grid.step + 1, 64); }
};
UI.pageOf = name => PAGE[name];
UI.frameTick = function () {
  UI.frame++;
  if (UI.cur === 'BOOT') { SCREENS.BOOT.draw(); g.present(); return; }
  const now = S.now(), t = tr(), shift = [0, 0, -24, 0, 0, 12, 12, 0][t.role] + 12 * UI.oct;
  for (const e of E.vis) if (!e.seen && e.t <= now) {
    e.seen = 1;
    if (e.kick) { UI.kick = e.t; continue; }
    UI.pulse[e.ti] = e.t;
    if (e.ti === UI.sel && !SCREENS[UI.cur].key) {
      const k = t.role < 2 || t.role === 7 ? e.n : e.n - 53 - shift;
      if (k >= 0 && k < 27) UI.keyLit[k] = performance.now() + 120;
    }
  }
  readAudio();
  const scr = SCREENS[UI.cur];
  g.cls(0); scr.draw(); topBar(scr.title()); knobBar(scr.knobs());
  if (UI.toast && T() < UI.toastT) { const w = UI.toast.length * 6 + 12; g.rect(120 - w / 2, 196, w, 15, 3); g.frame(120 - w / 2, 196, w, 15, 7); g.textC(120, 200, UI.toast, 0); }
  g.present();
};
UI.init = function (canvas) {
  g.attach(canvas);
  const sk = parseInt(store.get('pb_skin'), 10); if (sk >= 0) g.setSkin(sk);
  const loop = () => { try { UI.frameTick(); } catch (e) { console.error(e); } requestAnimationFrame(loop); };
  requestAnimationFrame(loop);
};
root.UI = UI;
})(window);
