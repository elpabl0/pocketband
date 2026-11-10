/* Pocket Band music brain. Pure and deterministic: no DOM, no audio.
 * Everything here is integer/table driven so it can be ported to C for the FM-1.
 * A pattern is 64 sixteenth-note steps; each step is null or a list of events
 * {n: midi note or drum pad, v: velocity, g: gate in steps, p: priority 0..1}.
 * Priority is what the live ENERGY control thins out: low-p events drop first. */
(function (root) {
'use strict';

function rng32(seed) {
  let a = seed >>> 0;
  return function () {
    a = (a + 0x6D2B79F5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
function hash(a, b) {
  let h = (Math.imul(a | 0, 0x9E3779B1) ^ Math.imul((b | 0) + 0x7F4A7C15, 0x85EBCA6B)) >>> 0;
  h ^= h >>> 13; h = Math.imul(h, 0x5bd1e995) >>> 0; h ^= h >>> 15;
  return (h >>> 0) / 4294967296;
}
const clamp = (v, lo, hi) => v < lo ? lo : v > hi ? hi : v;
const mod = (a, n) => ((a % n) + n) % n;
const pick = (arr, rnd) => arr[Math.floor(rnd() * arr.length)];

const SCALES = {
  MINOR: [0, 2, 3, 5, 7, 8, 10], MAJOR: [0, 2, 4, 5, 7, 9, 11], DORIAN: [0, 2, 3, 5, 7, 9, 10],
  PHRYGIAN: [0, 1, 3, 5, 7, 8, 10], MIXO: [0, 2, 4, 5, 7, 9, 10], LYDIAN: [0, 2, 4, 6, 7, 9, 11],
  HARMONIC: [0, 2, 3, 5, 7, 8, 11]
};
const SCALE_NAMES = Object.keys(SCALES);
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const ROLES = ['DRUMS', 'PERC', 'BASS', 'CHORD', 'PAD', 'LEAD', 'ARP', 'FX'];
const DRUM_PADS = [['kick', 'snare', 'clap', 'chh', 'ohh'], ['rim', 'shaker', 'tomlo', 'tomhi', 'cow']];
const PAD_LABELS = [['KICK', 'SNARE', 'CLAP', 'HAT', 'OPEN'], ['RIM', 'SHAKE', 'TOM L', 'TOM H', 'BELL']];
const SOUNDS = {
  0: ['808', '909', 'CHIP', 'LOFI', 'DUB'], 1: ['808', '909', 'CHIP', 'LOFI', 'DUB'],
  2: ['sub', 'fmbass', 'acid', 'reese', 'chipbass', 'slap'],
  3: ['ep', 'stab', 'organ', 'clav', 'chip', 'keys', 'pluck', 'brass'],
  4: ['pad', 'strings', 'glass', 'organ'],
  5: ['saw', 'sine', 'bell', 'chiplead', 'vibes', 'brass', 'whistle'],
  6: ['pluck', 'chip', 'bell', 'saw', 'vibes', 'keys'],
  7: ['fx']
};
const Z = '0000000000000000';

/* Genre profiles. Weight strings are 16 steps, 0 = never, 9 = always.
 * D = drum kit rows (kick, snare, clap, closed hat, open hat); P = percussion rows. */
const GENRES = [
  { name: 'HOUSE', tag: 'HOUSE', bpm: [120, 126], sw: [0, 8], scales: ['MINOR', 'DORIAN'], sev: .5, kit: '909', sp: 1,
    progs: [[0, 5, 2, 6], [0, 3, 5, 4], [0, 0, 5, 6], [0, 2, 6, 3]],
    D: { k: '9000900090009000', s: '0000000000000012', c: '0000900000009000', h: '5352535253525352', o: '0090009000900090' },
    P: { r: '0000000000100010', sh: '3636363636363636', t1: '0000000000000201', t2: '0000001000000000', cb: Z },
    bass: { w: '0070107000701072', oct: .15, fifth: .1, col: .05, gate: 2, snd: 'fmbass' },
    chord: { w: '0000700300070030', gate: 1, snd: 'stab' }, pad: 'pad',
    lead: { w: '7020503020705020', snd: 'saw' }, arp: { rate: 1, snd: 'pluck', w: '9099909990999099' }, on: '11110011' },
  { name: 'TECHNO', tag: 'TECHN', bpm: [128, 138], sw: [0, 4], scales: ['PHRYGIAN', 'MINOR'], sev: .1, kit: '909', sp: 1.1,
    progs: [[0, 0, 0, 0], [0, 0, 1, 0], [0, 0, 5, 6]],
    D: { k: '9000900090009000', s: Z, c: '0000700000007000', h: '5252525252525252', o: '0090009000900090' },
    P: { r: '0010000100100100', sh: Z, t1: '0000000000010010', t2: '0000000100000000', cb: '0000001000000010' },
    bass: { w: '0597059705970597', oct: .05, fifth: 0, col: .05, gate: 1, snd: 'fmbass' },
    chord: { w: '0000000300000090', gate: 1, snd: 'stab' }, pad: 'pad',
    lead: { w: '3000001000300010', snd: 'sine' }, arp: { rate: 1, snd: 'pluck', w: '9009009090090090' }, on: '11100110' },
  { name: 'ACID', tag: 'ACID', bpm: [124, 134], sw: [0, 8], scales: ['MINOR', 'PHRYGIAN'], sev: .2, kit: '909', sp: 1,
    progs: [[0, 0, 0, 0], [0, 0, 6, 0], [0, 0, 5, 6]],
    D: { k: '9000900090009000', s: '0000000000000021', c: '0000900000009000', h: '5252525252525252', o: '0090009000900090' },
    P: { r: '0000000100000010', sh: '2525252525252525', t1: '0000000000000030', t2: Z, cb: '0010000000100000' },
    bass: { w: '9577957795779577', oct: .2, fifth: .1, col: .25, gate: 1, slide: .25, snd: 'acid' },
    chord: { w: '0000000000900000', gate: 1, snd: 'stab' }, pad: 'pad',
    lead: { w: '3000200010002000', snd: 'sine' }, arp: { rate: 1, snd: 'pluck', w: '9090909090909090' }, on: '11100001' },
  { name: 'BOOM BAP', tag: 'BOOM', bpm: [84, 96], sw: [14, 24], scales: ['MINOR', 'DORIAN'], sev: .9, kit: 'LOFI', sp: .9,
    progs: [[0, 3, 6, 2], [0, 5, 3, 4], [1, 4, 0, 0]],
    D: { k: '9000000390200000', s: '0000900000009000', c: Z, h: '9050905090509050', o: '0000000000000020' },
    P: { r: '0000000000100000', sh: '3030303030303030', t1: Z, t2: Z, cb: Z },
    bass: { follow: 1, w: Z, oct: .1, fifth: .15, col: .1, gate: 0, snd: 'fmbass' },
    chord: { w: '9000000000300000', gate: 0, snd: 'ep' }, pad: 'strings',
    lead: { w: '5020302050203020', snd: 'vibes' }, arp: { rate: 2, snd: 'keys', w: '9090909090909090' }, on: '11110100' },
  { name: 'TRAP', tag: 'TRAP', bpm: [132, 150], sw: [0, 4], scales: ['MINOR', 'PHRYGIAN', 'HARMONIC'], sev: .3, kit: '808', sp: 1,
    progs: [[0, 0, 5, 4], [0, 5, 0, 6], [0, 3, 0, 4]],
    D: { k: '9000000000300900', s: '0000000090000003', c: '0000000090000000', h: '9292939292929396', o: '0000001000000000' },
    P: { r: '0000000000000100', sh: Z, t1: Z, t2: Z, cb: '0000000000001000' },
    bass: { follow: 1, w: Z, oct: .05, fifth: .1, col: .05, gate: 0, slide: .2, snd: 'sub' },
    chord: { w: '9000000000000000', gate: 0, snd: 'keys' }, pad: 'glass',
    lead: { w: '7000300050002030', snd: 'bell' }, arp: { rate: 2, snd: 'bell', w: '9090909090909090' }, on: '10110100' },
  { name: 'DRUM N BASS', tag: 'DNB', bpm: [168, 176], sw: [0, 6], scales: ['MINOR', 'DORIAN'], sev: .5, kit: '909', sp: 1.1,
    progs: [[0, 0, 5, 6], [0, 5, 2, 6], [0, 3, 5, 4]],
    D: { k: '9000000000930000', s: '0000900200009020', c: Z, h: '9090909090909090', o: '0000000000000030' },
    P: { r: '0010000100000100', sh: '3535353535353535', t1: Z, t2: Z, cb: Z },
    bass: { w: '9000000000300090', oct: .05, fifth: .15, col: .1, gate: 0, snd: 'reese' },
    chord: { w: '9000000000000000', gate: 0, snd: 'keys' }, pad: 'strings',
    lead: { w: '5000200030001000', snd: 'bell' }, arp: { rate: 2, snd: 'pluck', w: '9090909090909090' }, on: '11101100' },
  { name: 'DUB', tag: 'DUB', bpm: [68, 80], sw: [8, 16], scales: ['MINOR', 'DORIAN'], sev: .3, kit: 'DUB', sp: 1.7,
    progs: [[0, 0, 3, 3], [0, 6, 0, 6], [0, 3, 0, 4]],
    D: { k: '2000000090000000', s: '0000000090000000', c: Z, h: '9093909390939093', o: '0000000000000020' },
    P: { r: '0000900000001000', sh: '2020202020202020', t1: Z, t2: '0000000000000300', cb: Z },
    bass: { w: '9002300090020030', oct: 0, fifth: .25, col: .2, gate: 0, snd: 'sub' },
    chord: { w: '0090009000900090', gate: 1, snd: 'organ' }, pad: 'pad',
    lead: { w: '5000200030000000', snd: 'whistle' }, arp: { rate: 2, snd: 'pluck', w: '0090009000900090' }, on: '11110001' },
  { name: 'SYNTHWAVE', tag: 'SYNTH', bpm: [96, 116], sw: [0, 2], scales: ['MINOR', 'DORIAN'], sev: .3, kit: '808', sp: 1.2,
    progs: [[5, 6, 0, 0], [0, 5, 2, 6], [0, 6, 5, 6], [0, 2, 5, 4]],
    D: { k: '9000900090009000', s: '0000900000009000', c: Z, h: '9050905090509050', o: '0000000000000050' },
    P: { r: Z, sh: Z, t1: '0000000000003030', t2: '0000000000300000', cb: Z },
    bass: { w: '9292929292929292', oct: .1, fifth: .05, col: 0, gate: 1, snd: 'fmbass' },
    chord: { w: '9000000000000000', gate: 0, snd: 'brass' }, pad: 'strings',
    lead: { w: '7030503070305020', snd: 'saw' }, arp: { rate: 1, snd: 'pluck', w: '9999999999999999' }, on: '11101110' },
  { name: 'CHIPTUNE', tag: 'CHIP', bpm: [132, 158], sw: [0, 2], scales: ['MAJOR', 'MINOR'], sev: 0, kit: 'CHIP', sp: .5,
    progs: [[0, 4, 5, 3], [0, 3, 4, 4], [5, 3, 0, 4], [0, 5, 3, 4]],
    D: { k: '9000300090003030', s: '0000900000009000', c: Z, h: '9090909090909090', o: Z },
    P: { r: '0000000000100010', sh: Z, t1: '0000000000000030', t2: Z, cb: Z },
    bass: { w: '9090909090909090', oct: .5, fifth: .1, col: 0, gate: 1, snd: 'chipbass' },
    chord: { w: '9009009090090090', gate: 1, snd: 'chip' }, pad: 'organ',
    lead: { w: '9050705090507050', snd: 'chiplead' }, arp: { rate: 1, snd: 'chip', w: '9999999999999999' }, on: '10100110' },
  { name: 'AMBIENT', tag: 'AMBNT', bpm: [62, 80], sw: [0, 0], scales: ['MAJOR', 'LYDIAN'], sev: .8, kit: 'LOFI', sp: 1.9,
    progs: [[0, 3, 0, 4], [0, 5, 3, 4], [3, 0, 4, 5]],
    D: { k: '3000000000000000', s: Z, c: Z, h: '0010001000100010', o: Z },
    P: { r: Z, sh: '1010101010101010', t1: Z, t2: Z, cb: '0000000010000000' },
    bass: { w: '9000000000000000', oct: 0, fifth: 0, col: 0, gate: 0, snd: 'sub' },
    chord: { w: '9000000030000000', gate: 0, snd: 'keys' }, pad: 'glass',
    lead: { w: '5000200030001000', snd: 'bell' }, arp: { rate: 2, snd: 'bell', w: '9090009090000090' }, on: '00101110' },
  { name: 'FUNK', tag: 'FUNK', bpm: [104, 118], sw: [4, 12], scales: ['DORIAN', 'MIXO'], sev: .8, kit: '808', sp: .8,
    progs: [[0, 3, 0, 3], [1, 4, 0, 0], [0, 0, 3, 4], [0, 6, 3, 0]],
    D: { k: '9000000390200000', s: '0000900200009020', c: Z, h: '9597959795979597', o: '0000000000500050' },
    P: { r: Z, sh: '5353535353535353', t1: Z, t2: '0030000300300003', cb: '0000100000001000' },
    bass: { w: '9003090030900390', oct: .25, fifth: .15, col: .15, gate: 1, snd: 'slap' },
    chord: { w: '0090030090030900', gate: 1, snd: 'clav' }, pad: 'organ',
    lead: { w: '5000302050003020', snd: 'brass' }, arp: { rate: 2, snd: 'keys', w: '9090909090909090' }, on: '11110100' },
  { name: 'LO-FI', tag: 'LOFI', bpm: [70, 86], sw: [16, 26], scales: ['MAJOR', 'DORIAN'], sev: 1, kit: 'LOFI', sp: 1.2,
    progs: [[1, 4, 0, 5], [0, 5, 1, 4], [3, 2, 1, 0]],
    D: { k: '9000000290100000', s: '0000900000009000', c: Z, h: '9050905090509050', o: Z },
    P: { r: '0000000000001000', sh: '2020202020202020', t1: Z, t2: Z, cb: Z },
    bass: { follow: 1, w: Z, oct: .05, fifth: .2, col: .15, gate: 0, snd: 'sub' },
    chord: { w: '9000000000200000', gate: 0, snd: 'ep' }, pad: 'pad',
    lead: { w: '5010201040102010', snd: 'vibes' }, arp: { rate: 2, snd: 'keys', w: '9090009090900090' }, on: '10110100' },
  { name: 'DEMBOW', tag: 'DEMBO', bpm: [90, 100], sw: [0, 4], scales: ['MINOR', 'HARMONIC'], sev: .2, kit: '808', sp: .9,
    progs: [[0, 5, 2, 6], [0, 3, 4, 4], [0, 6, 5, 4]],
    D: { k: '9000900090009000', s: '0009009000090090', c: Z, h: '5050505050505050', o: '0000000000000030' },
    P: { r: '0009009000090090', sh: '3030303030303030', t1: Z, t2: Z, cb: '0000000000001000' },
    bass: { w: '9003009090030090', oct: .1, fifth: .15, col: .05, gate: 2, snd: 'sub' },
    chord: { w: '0009009000090090', gate: 1, snd: 'pluck' }, pad: 'pad',
    lead: { w: '7020302050203000', snd: 'whistle' }, arp: { rate: 2, snd: 'pluck', w: '9090909090909090' }, on: '11110100' },
  { name: 'GARAGE', tag: 'GARAG', bpm: [130, 136], sw: [12, 22], scales: ['MINOR', 'DORIAN'], sev: .7, kit: '909', sp: 1,
    progs: [[0, 5, 3, 4], [0, 2, 5, 6], [1, 4, 0, 0]],
    D: { k: '9000000300900000', s: '0000900000009000', c: '0000000000009000', h: '3090309030903092', o: '0000000000000030' },
    P: { r: '0010000100000010', sh: '2525252525252525', t1: Z, t2: Z, cb: Z },
    bass: { w: '9000030090300300', oct: .15, fifth: .1, col: .1, gate: 2, snd: 'reese' },
    chord: { w: '0030000900300009', gate: 1, snd: 'organ' }, pad: 'strings',
    lead: { w: '5020002030200020', snd: 'sine' }, arp: { rate: 2, snd: 'vibes', w: '9090909090909090' }, on: '11110100' }
];

const ROLE_VOL = [.9, .55, .85, .5, .4, .6, .42, .5];
const ROLE_SPACE = [0, .1, 0, .3, .5, .35, .4, .3];

/* ---- pitch helpers: everything melodic is stored as MIDI, derived from scale degrees ---- */
function rootOff(song) { return song.root > 6 ? song.root - 12 : song.root; }
function sn(song, deg) {
  const sc = SCALES[song.scale];
  return rootOff(song) + 12 * Math.floor(deg / 7) + sc[mod(deg, 7)];
}
function noteToDeg(song, n) {
  const sc = SCALES[song.scale], rel = n - rootOff(song);
  const oct = Math.floor(rel / 12), pc = rel - 12 * oct;
  let best = 0, bd = 99;
  for (let i = 0; i < 7; i++) { const d = Math.abs(sc[i] - pc); if (d < bd) { bd = d; best = i; } }
  if (12 - pc < bd) return (oct + 1) * 7;
  return oct * 7 + best;
}
function snap(song, n) { return sn(song, noteToDeg(song, n)); }
function fold(n, lo, hi) { while (n >= hi) n -= 12; while (n < lo) n += 12; return n; }
function snapChord(d, c) {
  for (let o = 0; o < 4; o++) {
    for (const s of [o, -o]) { const m = mod(d + s - c, 7); if (m === 0 || m === 2 || m === 4) return d + s; }
  }
  return d;
}
function chordName(song, c) {
  const sc = SCALES[song.scale];
  const r = sc[mod(c, 7)], t = mod(sc[mod(c + 2, 7)] - r, 12), f = mod(sc[mod(c + 4, 7)] - r, 12);
  const q = f === 6 ? 'DIM' : f === 8 ? '+' : t === 3 ? 'M' : '';
  return NOTE_NAMES[mod(song.root + r, 12)] + q;
}

/* ---- pattern helpers ---- */
const mk = () => new Array(64).fill(null);
function add(pat, s, ev) { (pat[s] || (pat[s] = [])).push(ev); }
function hits(w, rnd, f) {
  for (let s = 0; s < 16; s++) {
    const wv = w.charCodeAt(s) - 48;
    if (wv > 0) f(s, wv, wv >= 9 ? 1 : Math.min(.98, (wv / 9) * (0.55 + 0.6 * rnd())));
  }
}

/* ---- generators, one per band role ---- */
function genDrums(g, rnd, rows, fill) {
  const pat = mk(), keys = Object.keys(rows), bars = [[], []];
  for (let v = 0; v < 2; v++) keys.forEach((k, pad) => hits(rows[k] || Z, rnd, (s, wv, p) =>
    bars[v].push({ s, pad, p, v: wv < 4 ? 62 : (s % 4 === 0 ? 112 : 92) })));
  for (let b = 0; b < 4; b++) for (const h of bars[b & 1]) add(pat, b * 16 + h.s, { n: h.pad, v: h.v, g: 1, p: h.p });
  for (let s = 60; s < 64; s++) if (rnd() < .7) add(pat, s, { n: fill[Math.floor(rnd() * fill.length)], v: 70 + (s - 60) * 12, g: 1, p: .45 + rnd() * .3 });
  return pat;
}
function genBass(song, g, rnd) {
  const b = g.bass, pat = mk(), motif = [];
  hits(b.follow ? song.kickW : b.w, rnd, (s, wv, p) => {
    let off = 0; const r = rnd();
    if (s > 0) {
      if (r < b.oct) off = 7; else if (r < b.oct + b.fifth) off = 4;
      else if (r < b.oct + b.fifth + (b.col || 0)) off = [2, 6, -1, 3][Math.floor(rnd() * 4)];
    }
    motif.push({ s, off, p: b.follow ? Math.max(p, .5) : p, sl: rnd() < (b.slide || 0), acc: rnd() < .3 });
  });
  if (!motif.length) motif.push({ s: 0, off: 0, p: 1, sl: false, acc: true });
  for (let bar = 0; bar < 4; bar++) {
    const c = song.prog[bar], r0 = fold(sn(song, 21 + c), 28, 40);
    motif.forEach((m, i) => {
      const gate = b.gate || ((motif[i + 1] ? motif[i + 1].s : 16) - m.s);
      const n = r0 + sn(song, 21 + c + m.off) - sn(song, 21 + c);
      add(pat, bar * 16 + m.s, { n, v: m.acc ? 118 : 96, g: gate, p: m.p, sl: m.sl });
    });
  }
  if (rnd() < .6 && !pat[62]) add(pat, 62, { n: fold(sn(song, 21 + song.prog[0] - 1), 28, 40), v: 90, g: 2, p: .55 });
  return pat;
}
function genChord(song, g, rnd) {
  const pat = mk(), tones = song.sev ? [0, 2, 4, 6] : [0, 2, 4], on = [];
  hits(g.chord.w, rnd, (s, wv, p) => on.push({ s, p }));
  for (let bar = 0; bar < 4; bar++) {
    const c = song.prog[bar];
    on.forEach((o, i) => {
      const gate = g.chord.gate || ((on[i + 1] ? on[i + 1].s : 16) - o.s);
      for (const t of tones) add(pat, bar * 16 + o.s, { n: fold(sn(song, 28 + c + t), 53, 65), v: 84, g: gate, p: o.p });
    });
  }
  return pat;
}
function genPad(song, g, rnd) {
  const pat = mk(), tones = song.sev ? [0, 2, 4, 6] : [0, 2, 4], spread = rnd() < .5;
  for (let bar = 0; bar < 4; bar++) {
    const c = song.prog[bar];
    add(pat, bar * 16, { n: fold(sn(song, 28 + c), 48, 60), v: 66, g: 16, p: 1 });
    tones.forEach((t, i) => add(pat, bar * 16, { n: fold(sn(song, 28 + c + t), spread && i === 1 ? 67 : 60, spread && i === 1 ? 79 : 72), v: 62, g: 16, p: 1 }));
  }
  return pat;
}
function genLead(song, g, seed) {
  const pat = mk();
  function half(barOff, final) {
    const r = rng32(seed);   /* same seed twice: second half repeats the contour over new chords */
    let d = [0, 2, 4][Math.floor(r() * 3)] + (r() < .3 ? 7 : 0);
    const on = [];
    for (let bar = 0; bar < 2; bar++) hits(g.lead.w, r, (s, wv, p) => on.push({ s: bar * 16 + s, p }));
    on.forEach((o, i) => {
      const c = song.prog[barOff + (o.s >> 4)];
      const jump = [-2, -1, -1, 1, 1, 2, 0][Math.floor(r() * 7)], shortN = r() < .3;
      if (i > 0) d += jump;
      if (o.s % 4 === 0 || i === 0) d = snapChord(d, c);
      d = clamp(d, -2, 11);
      let gate = Math.min((on[i + 1] ? on[i + 1].s : 32) - o.s, 4), dd = d;
      if (shortN) gate = 1;
      if (final && i === on.length - 1) { dd = snapChord(d, c); gate = 4; }
      add(pat, barOff * 16 + o.s, { n: sn(song, 35 + dd), v: o.s % 4 === 0 ? 108 : 90, g: gate, p: o.s % 4 === 0 ? Math.max(o.p, .75) : o.p });
    });
  }
  half(0, false); half(2, true);
  return pat;
}
function genArp(song, g, rnd) {
  const a = g.arp, pat = mk(), tones = song.sev ? [0, 2, 4, 6] : [0, 2, 4, 7];
  const seq = pick([[0, 1, 2, 3], [3, 2, 1, 0], [0, 1, 2, 3, 2, 1], [0, 2, 1, 3], [0, 3, 1, 2, 0, 2, 1, 3]], rnd);
  const hi = rnd() < .4 ? 7 : 0;
  for (let bar = 0; bar < 4; bar++) {
    const c = song.prog[bar]; let i = 0;
    for (let s = 0; s < 16; s += a.rate, i++) {
      if (a.w.charCodeAt(s) === 48) continue;
      const strong = a.rate === 1 ? s % 2 === 0 : s % 4 === 0;
      add(pat, bar * 16 + s, { n: fold(sn(song, 35 + c + tones[seq[i % seq.length]] + (i % 8 > 3 ? hi : 0)), 60, 86), v: s % 4 === 0 ? 100 : 80, g: a.rate, p: strong ? 1 : .55 });
    }
  }
  return pat;
}
function genFx(rnd) {
  const pat = mk();
  add(pat, 0, { n: 0, v: 100, g: 4, p: .9 });
  add(pat, 48, { n: 1, v: 100, g: 16, p: .8 });
  if (rnd() < .6) add(pat, 32 + 4 * Math.floor(rnd() * 3), { n: 2, v: 90, g: 2, p: .5 });
  return pat;
}
function genPattern(song, role, gi, seed) {
  const g = GENRES[gi], rnd = rng32(seed);
  switch (role) {
    case 0: return genDrums(g, rnd, g.D, [1, 1, 1, 0]);
    case 1: return genDrums(g, rnd, g.P, [2, 3, 2, 3]);
    case 2: return genBass(song, g, rnd);
    case 3: return genChord(song, g, rnd);
    case 4: return genPad(song, g, rnd);
    case 5: return genLead(song, g, seed);
    case 6: return genArp(song, g, rnd);
    default: return genFx(rnd);
  }
}
function soundFor(role, gi) {
  const g = GENRES[gi];
  return role < 2 ? g.kit : role === 2 ? g.bass.snd : role === 3 ? g.chord.snd : role === 4 ? g.pad : role === 5 ? g.lead.snd : role === 6 ? g.arp.snd : 'fx';
}
function makeTrack(song, role, gi, seed) {
  const g = GENRES[gi];
  return { role, g: gi, seed, snd: soundFor(role, gi), mute: false, vol: ROLE_VOL[role],
    mac: { bright: .5, punch: .5, weird: 0, space: clamp(ROLE_SPACE[role] * g.sp, 0, 1) },
    A: genPattern(song, role, gi, seed), B: genPattern(song, role, gi, seed + 7717) };
}

/* A whole sketch from four numbers: two genres, how far between them, and a seed. */
function makeSong(par) {
  const gA = par.gA % GENRES.length, gB = par.gB % GENRES.length, blend = clamp(par.blend | 0, 0, 7), t = blend / 14;
  const rnd = rng32(Math.imul(par.seed + 1, 7919) + gA * 131 + gB * 17 + blend);
  const A = GENRES[gA], B = GENRES[gB];
  const rg = r => r[0] + rnd() * (r[1] - r[0]);
  const song = { gA, gB, blend, seed: par.seed };
  song.bpm = Math.round(rg(A.bpm) * (1 - t) + rg(B.bpm) * t);
  song.swing = (rg(A.sw) * (1 - t) + rg(B.sw) * t) / 100;
  song.root = Math.floor(rnd() * 12);
  song.scale = pick(A.scales, rnd);
  const pg = rnd() < t ? B : A;
  song.progs = pg.progs; song.progIdx = Math.floor(rnd() * pg.progs.length);
  song.prog = pg.progs[song.progIdx].slice();
  song.sev = rnd() < A.sev;
  const src = [];
  for (let r = 0; r < 8; r++) src.push(rnd() < t ? gB : gA);
  if (blend > 0 && src.every(x => x === gA)) src[5] = src[2] = gB;
  song.kickW = GENRES[src[0]].D.k;
  song.tracks = [];
  for (let r = 0; r < 8; r++) {
    const tr = makeTrack(song, r, src[r], Math.floor(rnd() * 0x7fffffff));
    tr.mute = GENRES[src[r]].on[r] === '0';
    song.tracks.push(tr);
  }
  return song;
}
function reharm(song) {      /* new chord progression: melodic parts are rebuilt from their own seeds */
  song.progIdx = (song.progIdx + 1) % song.progs.length;
  song.prog = song.progs[song.progIdx].slice();
  for (const tr of song.tracks) if (tr.role >= 2 && tr.role <= 6) {
    tr.A = genPattern(song, tr.role, tr.g, tr.seed); tr.B = genPattern(song, tr.role, tr.g, tr.seed + 7717);
  }
}
function retune(song, root, scale) {   /* change key or scale, keeping every note on its scale degree */
  const maps = [];
  for (const tr of song.tracks) if (tr.role >= 2 && tr.role <= 6) for (const pat of [tr.A, tr.B])
    for (const st of pat) if (st) for (const ev of st) maps.push([ev, noteToDeg(song, ev.n)]);
  song.root = mod(root, 12); song.scale = scale;
  for (const [ev, d] of maps) ev.n = sn(song, d);
}
function newB(song, salt) {
  for (const tr of song.tracks) tr.B = genPattern(song, tr.role, tr.g, tr.seed + 7717 + salt * 101);
}

/* ---- recommendations: score how well a candidate part sits with the rest of the band ---- */
function onsets(pat, thr) {
  const o = [];
  for (let s = 0; s < 64; s++) if (pat[s] && pat[s].some(e => e.p >= thr)) o.push(s);
  return o;
}
function fit(song, ti, pat, energy) {
  const tr = song.tracks[ti], role = tr.role, thr = 1 - energy, on = onsets(pat, thr);
  if (!on.length) return 40;
  const target = [36, 30, 22, 10, 4, 16, 40, 2][role], dens = 1 - Math.min(1, Math.abs(on.length - target) / (target + 6));
  let score = 45 + 22 * dens;
  if (role < 2 || role === 7) return Math.round(clamp(score + 22 + 8 * hash(tr.seed, on.length), 40, 99));
  let clash = 0, others = 0, kick = 0;
  const kicks = new Set(onsets(song.tracks[0].A, .7).filter(s => song.tracks[0].A[s].some(e => e.n === 0)));
  for (const s of on) if (kicks.has(s)) kick++;
  song.tracks.forEach((o, i) => {
    if (i === ti || o.mute || o.role < 2 || o.role === 7 || o.role === 4) return;
    const os = new Set(onsets(o.A, thr)); others++;
    let c = 0; for (const s of on) if (os.has(s)) c++;
    clash += c / on.length;
  });
  if (others) score += (role === 2 ? 4 : 12) * (1 - clash / others);
  if (role === 2) score += 12 * (kick / on.length);
  let ct = 0, cn = 0, sum = 0, cnt = 0;
  for (const s of on) for (const e of pat[s]) {
    sum += e.n; cnt++;
    if (s % 4 === 0) { cn++; const m = mod(noteToDeg(song, e.n) - song.prog[s >> 4], 7); if (m === 0 || m === 2 || m === 4) ct++; }
  }
  if (cn) score += 12 * (ct / cn);
  const mean = sum / cnt; let near = 99;
  song.tracks.forEach((o, i) => {
    if (i === ti || o.mute || o.role < 2 || o.role === 7) return;
    let s2 = 0, c2 = 0; for (const st of o.A) if (st) for (const e of st) { s2 += e.n; c2++; }
    if (c2) near = Math.min(near, Math.abs(mean - s2 / c2));
  });
  score += near > 6 ? 8 : near > 3 ? 4 : 0;
  return Math.round(clamp(score, 40, 99));
}
function candidates(song, ti, salt, energy) {
  const tr = song.tracks[ti], rnd = rng32(tr.seed + salt * 977 + 13), out = [];
  const other = song.gB !== song.gA ? song.gB : Math.floor(rnd() * GENRES.length);
  let wild = Math.floor(rnd() * GENRES.length);
  if (wild === tr.g) wild = (wild + 3) % GENRES.length;
  for (const gi of [tr.g, other, wild]) {
    const seed = Math.floor(rnd() * 0x7fffffff), pat = genPattern(song, tr.role, gi, seed);
    out.push({ g: gi, seed, pat, snd: soundFor(tr.role, gi), fit: fit(song, ti, pat, energy) });
  }
  return out.sort((a, b) => b.fit - a.fit);
}

/* ---- evolution: small mutations the player keeps or kills ---- */
const OPS = ['ADD', 'DROP', 'NUDGE', 'PITCH', 'ECHO'];
function mutate(song, ti, rnd, op) {
  const tr = song.tracks[ti], pat = tr.A, drum = tr.role < 2;
  const filled = [], empty = [];
  for (let s = 0; s < 64; s++) (pat[s] && pat[s].length ? filled : empty).push(s);
  if (op === 'ADD' && empty.length) {
    const ev8 = empty.filter(s => s % 2 === 0), s = pick(ev8.length && rnd() < .7 ? ev8 : empty, rnd), c = song.prog[s >> 4];
    if (drum) add(pat, s, { n: pick([0, 3, 3, 1, 4], rnd), v: 84, g: 1, p: .8 });
    else { const base = tr.role === 2 ? 21 : tr.role === 3 ? 28 : 35; add(pat, s, { n: sn(song, base + c + pick([0, 2, 4, 7], rnd)), v: 92, g: tr.role === 4 ? 8 : 2, p: .8 }); }
    return 'ADD';
  }
  if (op === 'DROP' && filled.length > 3) { pat[pick(filled.filter(s => s % 16 !== 0), rnd) || filled[1]] = null; return 'DROP'; }
  if (op === 'NUDGE' && filled.length) {
    const s = pick(filled, rnd), to = mod(s + (rnd() < .5 ? 1 : -1), 64);
    if (!pat[to]) { pat[to] = pat[s]; pat[s] = null; return 'NUDGE'; }
  }
  if (op === 'PITCH' && filled.length) {
    const s = pick(filled, rnd), d = rnd() < .5 ? 1 : -1;
    for (const e of pat[s]) e.n = drum ? mod(e.n + d, 5) : sn(song, noteToDeg(song, e.n) + d);
    return 'PITCH';
  }
  const from = Math.floor(rnd() * 4), to = (from + 1 + Math.floor(rnd() * 3)) % 4;
  for (let s = 0; s < 16; s++) {
    const src = pat[from * 16 + s];
    pat[to * 16 + s] = src ? src.map(e => { const c = Object.assign({}, e); if (!drum && tr.role !== 7) c.n = sn(song, noteToDeg(song, e.n) + song.prog[to] - song.prog[from]); return c; }) : null;
  }
  return 'ECHO';
}

/* ---- passwords: the whole sketch in six characters ---- */
const ALPHA = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
function encode(song) {
  let v = song.gA + song.gB * 16 + song.blend * 256 + (song.seed & 0x7ffff) * 2048, s = '';
  for (let i = 0; i < 6; i++) { s += ALPHA[v % 32]; v = Math.floor(v / 32); }
  return s;
}
function decode(str) {
  let v = 0;
  for (let i = 5; i >= 0; i--) { const k = ALPHA.indexOf(str[i]); if (k < 0) return null; v = v * 32 + k; }
  const gA = v % 16, gB = Math.floor(v / 16) % 16, blend = Math.floor(v / 256) % 8, seed = Math.floor(v / 2048);
  if (gA >= GENRES.length || gB >= GENRES.length) return null;
  return { gA, gB, blend, seed };
}

root.Brain = { rng32, hash, clamp, mod, SCALES, SCALE_NAMES, NOTE_NAMES, ROLES, DRUM_PADS, PAD_LABELS, SOUNDS, GENRES, OPS, ALPHA,
  sn, snap, noteToDeg, chordName, makeSong, makeTrack, genPattern, soundFor, reharm, retune, newB, fit, candidates, mutate, encode, decode };
})(typeof window !== 'undefined' ? window : globalThis);
