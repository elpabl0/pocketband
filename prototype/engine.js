/* Pocket Band engine: transport, live performance moves, sections, song mode, evolution.
 * On the FM-1 this layer would write into the device's own sequencer between bars. */
(function (root) {
'use strict';
const B = root.Brain, S = root.Sound;

/* A section is a band line-up (which members are awake) plus a drum mode and an energy lift. */
const SECTIONS = {
  INTRO: { m: [1, 1, 0, 1, 1, 0, 1, 0], dm: 'hats', e: -.15 },
  GROOVE: { m: [1, 1, 1, 1, 0, 0, 0, 0], dm: 'full', e: 0 },
  BUILD: { m: [1, 1, 0, 1, 1, 0, 1, 0], dm: 'roll', e: 0 },
  DROP: { m: [1, 1, 1, 1, 1, 1, 1, 1], dm: 'full', e: .15 },
  BREAK: { m: [0, 0, 0, 1, 1, 1, 0, 0], dm: 'full', e: -.1 },
  OUTRO: { m: [1, 1, 1, 0, 1, 0, 0, 0], dm: 'full', e: -.2 }
};
const CHAIN = [['INTRO', 4], ['GROOVE', 8], ['BUILD', 2], ['DROP', 8], ['BREAK', 4], ['BUILD', 2], ['DROP', 8], ['OUTRO', 4]];

const E = {
  song: null, playing: false, gs: 0, next: 0, timer: null,
  energy: .6, morph: 0, filter: 0, section: 'JAM', queued: null, queuedSong: null, drumMode: 'full', secE: 0,
  songMode: false, auto: false, chainIdx: 0, chainLeft: 0,
  held: { fill: false, stut: 0, echo: false, lowcut: false }, stutStart: -1, stutGs: 0, build: null,
  preview: null, rec: false, vis: [], clock: [], lastNote: [], kickT: -9,
  evolve: { rate: 0, last: null, taste: { ADD: 1, DROP: 1, NUDGE: 1, PITCH: 1, ECHO: 1 }, n: 0 },
  onAuto: null, onChange: null, SECTIONS, CHAIN
};

E.applyMix = function () {
  if (!E.song) return;
  S.setBpm(E.song.bpm);
  E.song.tracks.forEach((t, i) => S.setTrack(i, t.vol, t.mac.space));
};
function install(song) {
  E.song = song; E.queuedSong = null; E.section = 'JAM'; E.drumMode = 'full'; E.secE = 0; E.build = null; E.preview = null;
  E.evolve.last = null; E.lastNote = [];
  E.applyMix(); S.setFilter(E.filter);
  if (E.onChange) E.onChange();
}
E.load = function (song) { if (E.playing) E.queuedSong = song; else install(song); };
E.start = function () {
  if (!E.song || E.playing) return;
  S.resume(); E.playing = true; E.gs = 0; E.next = S.now() + .08; E.stutStart = -1;
  if (E.songMode) { E.chainIdx = -1; E.chainLeft = 0; }
  E.timer = setInterval(tick, 25); tick();
};
E.stop = function () { E.playing = false; clearInterval(E.timer); E.build = null; S.setFilter(E.filter); E.clock.length = 0; };
E.toggle = () => E.playing ? E.stop() : E.start();

function tick() {
  if (!E.playing) return;
  const now = S.now();
  if (E.next < now - .3) E.next = now + .05;      /* tab was asleep: resync rather than burst */
  while (E.next < now + .12) { sched(E.gs, E.next); E.next += 60 / E.song.bpm / 4; E.gs++; }
  while (E.vis.length && E.vis[0].t < now - 5) E.vis.shift();
}
function applySection(name, gs, t) {
  const sec = SECTIONS[name]; if (!sec) return;
  E.section = name; E.drumMode = sec.dm; E.secE = sec.e;
  E.song.tracks.forEach(tr => { tr.mute = !sec.m[tr.role]; });
  const sd = 60 / E.song.bpm / 4;
  if (name === 'BUILD') { E.build = { start: gs, len: 32 }; S.fx(7, 'riser', t, 32 * sd); }
  if (name === 'DROP') S.fx(7, 'impact', t, 1);
}
function onBar(gs, t) {
  if (E.queuedSong) install(E.queuedSong);
  if (E.queued) { applySection(E.queued, gs, t); E.queued = null; }
  else if (E.songMode && --E.chainLeft <= 0) {
    E.chainIdx++;
    if (E.chainIdx >= CHAIN.length) {
      E.chainIdx = 0;
      if (E.auto && E.onAuto) { const s = E.onAuto(); if (s) install(s); }
    }
    E.chainLeft = CHAIN[E.chainIdx][1];
    applySection(CHAIN[E.chainIdx][0], gs, t);
  }
  const ev = E.evolve;
  if (ev.rate && gs > 0 && (gs / 16) % ev.rate === 0) doMutate();
}
function doMutate() {
  const ev = E.evolve, awake = E.song.tracks.map((t, i) => i).filter(i => !E.song.tracks[i].mute && E.song.tracks[i].role !== 7);
  if (!awake.length) return;
  const rnd = B.rng32(E.song.seed + (++ev.n) * 7919 + E.gs);
  const ti = awake[Math.floor(rnd() * awake.length)];
  let tot = 0; for (const o of B.OPS) tot += ev.taste[o];
  let r = rnd() * tot, op = B.OPS[0];
  for (const o of B.OPS) { r -= ev.taste[o]; if (r <= 0) { op = o; break; } }
  const undo = JSON.stringify(E.song.tracks[ti].A);
  ev.last = { ti, op: B.mutate(E.song, ti, rnd, op), undo, at: S.now() };
}
E.judge = function (keep) {
  const ev = E.evolve, l = ev.last; if (!l) return;
  if (keep) ev.taste[l.op] = Math.min(4, ev.taste[l.op] + .5);
  else { E.song.tracks[l.ti].A = JSON.parse(l.undo); ev.taste[l.op] = Math.max(.25, ev.taste[l.op] - .35); }
  ev.last = null;
};
E.stepEv = function (ti, pos) {
  const tr = E.song.tracks[ti];
  if (E.preview && E.preview.ti === ti) return E.preview.pat[pos];
  return (E.morph > 0 && B.hash(ti * 64 + pos, 7) < E.morph ? tr.B : tr.A)[pos];
};
function rollEv(k, len) {
  const out = [], s = k & 15, last = k >= len - 16, v = 55 + 65 * (k / len);
  if (!last && s % 4 === 0) out.push({ n: 0, v: 110, g: 1, p: 1 });
  if (last || s % 2 === 0) out.push({ n: 1, v, g: 1, p: 1, dbl: k >= len - 4 });
  return out;
}
function fillEv(pos) {
  const s = pos & 15, out = [{ n: 1, v: s % 2 ? 78 : 104, g: 1, p: 1 }];
  if (s % 4 === 0) out.push({ n: 0, v: 112, g: 1, p: 1 });
  if (s === 14) out.push({ n: 4, v: 100, g: 1, p: 1 });
  return out;
}
function sched(gs, t0) {
  const song = E.song;
  if (E.build && gs - E.build.start >= E.build.len) { E.build = null; S.setFilter(E.filter); E.queued = E.songMode ? null : 'DROP'; if (E.songMode) E.chainLeft = 0; }
  if (gs % 16 === 0) onBar(gs, t0);
  const sd = 60 / E.song.bpm / 4, t = t0 + ((gs & 1) ? E.song.swing * sd : 0), pos = gs % 64;
  let play = pos;
  if (E.held.stut) {
    if (E.stutStart < 0) { E.stutStart = pos; E.stutGs = gs; }
    play = (E.stutStart + ((gs - E.stutGs) % E.held.stut)) % 64;
  } else E.stutStart = -1;
  E.clock.push({ t, gs, pos: play }); if (E.clock.length > 48) E.clock.shift();
  if (E.build) S.setFilter(Math.max(E.filter, ((gs - E.build.start) / E.build.len) * .6));
  const thr = 1 - B.clamp(E.energy + E.secE, 0, 1), tracks = E.song.tracks;
  for (let ti = 0; ti < 8; ti++) {
    const tr = tracks[ti], prev = E.preview && E.preview.ti === ti;
    if (tr.mute && !prev) continue;
    let evs;
    if (tr.role === 0 && E.build) evs = rollEv(gs - E.build.start, E.build.len);
    else if (tr.role === 0 && E.held.fill) evs = fillEv(pos);
    else evs = E.stepEv(ti, play);
    if (!evs) continue;
    for (const ev of evs) {
      if (ev.p < thr) continue;
      if (tr.role === 0 && E.drumMode === 'hats' && ev.n < 3) continue;
      trigger(ti, tr, ev, t, sd, prev ? E.preview.snd : tr.snd);
    }
  }
  if (song !== E.song) return;
}
function trigger(ti, tr, ev, t, sd, snd) {
  if (tr.role < 2) {
    S.drum(ti, snd, B.DRUM_PADS[tr.role][ev.n], ev.v, t, tr.mac);
    if (ev.dbl) S.drum(ti, snd, B.DRUM_PADS[tr.role][ev.n], ev.v, t + sd / 2, tr.mac);
    if (tr.role === 0 && ev.n === 0) E.vis.push({ t, ti, n: -1, g: 0, kick: 1 });
  } else if (tr.role === 7) S.fx(ti, ['impact', 'riser', 'blip'][ev.n] || 'blip', t, ev.g * sd);
  else { S.synth(ti, snd, tr.mac, ev.n, ev.v, t, ev.g * sd * .92, ev.sl ? E.lastNote[ti] : 0); E.lastNote[ti] = ev.n; }
  E.vis.push({ t, ti, n: ev.n, g: ev.g * sd });
}
E.posNow = function () {
  const now = S.now();
  for (let i = E.clock.length - 1; i >= 0; i--) if (E.clock[i].t <= now) return E.clock[i].pos;
  return -1;
};
E.gsNow = function () {
  const now = S.now();
  for (let i = E.clock.length - 1; i >= 0; i--) if (E.clock[i].t <= now) return E.clock[i].gs;
  return -1;
};
/* Live playing from the keys. Notes are pulled into the scale, so nothing sounds wrong. */
E.live = function (ti, key, octave) {
  if (!E.song || !S.ready) return -1;
  const tr = E.song.tracks[ti], t = S.now() + .005; let ev;
  if (tr.role < 2) { ev = { n: key % 5, v: 110, g: 1, p: 1 }; S.drum(ti, tr.snd, B.DRUM_PADS[tr.role][ev.n], 110, t, tr.mac); }
  else if (tr.role === 7) { ev = { n: key % 3, v: 100, g: 4, p: 1 }; S.fx(ti, ['impact', 'riser', 'blip'][ev.n], t, .8); }
  else {
    const shift = [0, 0, -24, 0, 0, 12, 12][tr.role] + 12 * octave;
    ev = { n: B.snap(E.song, 53 + key + shift), v: 104, g: 2, p: 1 };
    S.synth(ti, tr.snd, tr.mac, ev.n, 104, t, .28, 0);
  }
  E.vis.push({ t, ti, n: ev.n, g: .2 });
  if (E.rec && E.playing) {
    const pos = E.posNow();
    if (pos >= 0) { const A = tr.A; if (tr.role < 2) { A[pos] = (A[pos] || []).filter(e => e.n !== ev.n); A[pos].push(ev); } else A[pos] = [ev]; }
  }
  return ev.n;
};
E.setFilter = x => { E.filter = x; if (!E.build) S.setFilter(x); };
E.setHeld = function (k, v) {
  E.held[k] = v;
  if (k === 'echo') S.setEcho(v);
  if (k === 'lowcut') S.setLowCut(v);
};
E.oneShot = kind => { if (S.ready) S.fx(7, kind, S.now() + .005, 60 / (E.song ? E.song.bpm : 120) * 2); };
root.Engine = E;
})(window);
