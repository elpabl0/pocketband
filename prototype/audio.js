/* Pocket Band audio: a Web Audio stand-in for the FM-1's engines (FM, acid bass, 808, chip, pluck).
 * It is an approximation for the prototype; on the device the real engines play. */
(function (root) {
'use strict';
const A = { ctx: null, ready: false };
let tin = [], dsend = [], rsend = [], dry, delay, fb, lp, hp, hp2, comp, master, throwG, noiseBuf;

/* o: oscillator, o2: detune of a second oscillator (cents), fm: [ratio, index, decay s],
 * f: [cutoff Hz, env amount, Q, env decay s], a/d/s/r envelope, l: level trim */
const PRESETS = {
  sub: { o: 'sine', fm: [1, .5, .1], a: .005, d: .3, s: .8, r: .12, l: 1.1 },
  fmbass: { o: 'sine', fm: [1, 2.5, .12], a: .003, d: .25, s: .4, r: .08, l: 1 },
  acid: { o: 'sawtooth', f: [500, 5, 12, .14], a: .003, d: .2, s: .3, r: .05, l: .55 },
  reese: { o: 'sawtooth', o2: 18, f: [450, 1, 2, .4], a: .02, d: .3, s: .9, r: .15, l: .6 },
  chipbass: { o: 'triangle', a: .002, d: .1, s: .8, r: .03, l: 1 },
  slap: { o: 'square', f: [900, 3, 4, .08], a: .002, d: .15, s: .2, r: .05, l: .55 },
  ep: { o: 'sine', fm: [1, 1.2, .5], a: .004, d: .9, s: .25, r: .3, l: .8 },
  stab: { o: 'sawtooth', o2: 12, f: [1800, 2, 2, .12], a: .004, d: .18, s: .15, r: .1, l: .4 },
  organ: { o: 'sine', fm: [2, .8, 9], a: .008, d: .1, s: .9, r: .06, l: .6 },
  clav: { o: 'square', f: [2500, 2, 3, .07], a: .002, d: .14, s: .05, r: .05, l: .4 },
  chip: { o: 'square', a: .002, d: .08, s: .6, r: .03, l: .3 },
  keys: { o: 'triangle', fm: [3, .6, .2], a: .004, d: .5, s: .2, r: .2, l: .8 },
  pluck: { o: 'sawtooth', f: [900, 5, 3, .09], a: .002, d: .2, s: 0, r: .1, l: .5 },
  brass: { o: 'sawtooth', f: [700, 3, 1, .25], a: .04, d: .2, s: .8, r: .1, l: .4 },
  pad: { o: 'sawtooth', o2: 14, f: [900, 0, 1, 1], a: .35, d: .5, s: .8, r: .7, l: .3 },
  strings: { o: 'sawtooth', o2: 9, f: [2200, 0, .7, 1], a: .2, d: .4, s: .85, r: .5, l: .28 },
  glass: { o: 'sine', fm: [3.5, 1.2, 1.5], a: .15, d: .8, s: .6, r: .9, l: .55 },
  saw: { o: 'sawtooth', o2: 7, f: [2400, 1.5, 2, .2], a: .008, d: .2, s: .7, r: .12, l: .38 },
  sine: { o: 'sine', fm: [1, .3, .3], a: .02, d: .2, s: .8, r: .15, l: .8 },
  bell: { o: 'sine', fm: [3.5, 3, .6], a: .002, d: .9, s: 0, r: .6, l: .6 },
  chiplead: { o: 'square', a: .003, d: .05, s: .8, r: .04, l: .3, vib: 6 },
  vibes: { o: 'sine', fm: [4, 1.2, .15], a: .003, d: .7, s: 0, r: .4, l: .8 },
  whistle: { o: 'triangle', a: .03, d: .1, s: .9, r: .1, l: .7, vib: 5 }
};
const KITS = {
  '808': { kf: 46, kd: .5, ks: 5, sd: .16, st: 180, hf: 8000, hd: .04 },
  '909': { kf: 54, kd: .28, ks: 7, sd: .2, st: 200, hf: 9500, hd: .05 },
  'CHIP': { kf: 62, kd: .1, ks: 6, sd: .07, st: 0, hf: 5500, hd: .02, chip: 1 },
  'LOFI': { kf: 52, kd: .3, ks: 4, sd: .13, st: 170, hf: 5000, hd: .035 },
  'DUB': { kf: 50, kd: .36, ks: 3.5, sd: .11, st: 230, hf: 7000, hd: .03 }
};

A.init = function () {
  if (A.ctx) return;
  const C = window.AudioContext || window.webkitAudioContext;
  const ctx = A.ctx = new C();
  noiseBuf = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate);
  const nd = noiseBuf.getChannelData(0);
  for (let i = 0; i < nd.length; i++) nd[i] = Math.random() * 2 - 1;

  dry = ctx.createGain();
  delay = ctx.createDelay(2); fb = ctx.createGain(); fb.gain.value = .42;
  const dlp = ctx.createBiquadFilter(); dlp.type = 'lowpass'; dlp.frequency.value = 3200;
  delay.connect(dlp); dlp.connect(fb); fb.connect(delay);
  const rev = ctx.createConvolver(), ir = ctx.createBuffer(2, ctx.sampleRate * 2.4, ctx.sampleRate);
  for (let c = 0; c < 2; c++) { const d = ir.getChannelData(c); for (let i = 0; i < d.length; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / d.length, 3.2); }
  rev.buffer = ir;
  const revOut = ctx.createGain(); revOut.gain.value = .7; rev.connect(revOut);
  throwG = ctx.createGain(); throwG.gain.value = 0; dry.connect(throwG); throwG.connect(delay);

  lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 20000;
  hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 20;
  hp2 = ctx.createBiquadFilter(); hp2.type = 'highpass'; hp2.frequency.value = 20;
  comp = ctx.createDynamicsCompressor(); comp.threshold.value = -14; comp.ratio.value = 5; comp.attack.value = .004; comp.release.value = .16;
  master = ctx.createGain(); master.gain.value = .8;
  A.analyser = ctx.createAnalyser(); A.analyser.fftSize = 512; A.analyser.smoothingTimeConstant = .7;
  dry.connect(lp); delay.connect(lp); revOut.connect(lp);
  lp.connect(hp); hp.connect(hp2); hp2.connect(comp); comp.connect(master); master.connect(A.analyser); A.analyser.connect(ctx.destination);

  for (let i = 0; i < 8; i++) {
    const g = ctx.createGain(), d = ctx.createGain(), r = ctx.createGain();
    g.gain.value = .6; d.gain.value = 0; r.gain.value = 0;
    g.connect(dry); g.connect(d); d.connect(delay); g.connect(r); r.connect(rev);
    tin.push(g); dsend.push(d); rsend.push(r);
  }
  A.ready = true;
};
A.resume = () => { if (A.ctx && A.ctx.state !== 'running') A.ctx.resume(); };
A.now = () => A.ctx ? A.ctx.currentTime : 0;
A.setMaster = v => { if (A.ready) master.gain.setTargetAtTime(v, A.ctx.currentTime, .02); };
A.setBpm = bpm => { if (A.ready) delay.delayTime.setTargetAtTime(60 / bpm * .75, A.ctx.currentTime, .05); };
A.setTrack = (ti, vol, space) => {
  if (!A.ready) return; const t = A.ctx.currentTime;
  tin[ti].gain.setTargetAtTime(vol, t, .02);
  dsend[ti].gain.setTargetAtTime(space * .4, t, .02); rsend[ti].gain.setTargetAtTime(space * .65, t, .02);
};
/* One DJ filter: negative closes a low-pass, positive opens a high-pass, zero is flat. */
A.setFilter = x => {
  if (!A.ready) return; const t = A.ctx.currentTime;
  lp.frequency.setTargetAtTime(x < 0 ? 20000 * Math.pow(2, x * 7) : 20000, t, .03);
  hp.frequency.setTargetAtTime(x > 0 ? 20 * Math.pow(2, x * 8) : 20, t, .03);
  lp.Q.value = hp.Q.value = 1 + Math.abs(x) * 5;
};
A.setEcho = on => { if (A.ready) { throwG.gain.setTargetAtTime(on ? .9 : 0, A.ctx.currentTime, .01); fb.gain.setTargetAtTime(on ? .72 : .42, A.ctx.currentTime, .05); } };
A.setLowCut = on => { if (A.ready) hp2.frequency.setTargetAtTime(on ? 380 : 20, A.ctx.currentTime, .02); };

function env(g, t, peak, a, d, s, end, r) {
  g.gain.setValueAtTime(0, t);
  g.gain.linearRampToValueAtTime(peak, t + a);
  g.gain.setTargetAtTime(peak * s, t + a, Math.max(.005, d / 3));
  g.gain.setTargetAtTime(0, end, Math.max(.005, r / 3));
}
A.synth = function (ti, snd, mac, midi, vel, t, dur, slideFrom) {
  if (!A.ready) return;
  const ctx = A.ctx, P = PRESETS[snd] || PRESETS.saw, m = mac || { bright: .5, punch: .5, weird: 0 };
  const f = 440 * Math.pow(2, (midi - 69) / 12);
  const aMul = Math.pow(2, (.5 - m.punch) * 4), dMul = Math.pow(2, (.5 - m.punch) * 2), bMul = Math.pow(2, (m.bright - .5) * 4);
  const a = Math.max(.002, P.a * aMul), d = P.d * dMul, r = P.r * dMul;
  const end = t + Math.max(dur, a + .02), stop = end + r * 4 + .05;
  const amp = ctx.createGain();
  env(amp, t, (vel / 127) * P.l * .5, a, d, P.s, end, r);
  let out = amp;
  if (P.f) {
    const fl = ctx.createBiquadFilter(); fl.type = 'lowpass'; fl.Q.value = P.f[2];
    const cut = Math.min(16000, P.f[0] * bMul);
    fl.frequency.setValueAtTime(Math.min(18000, cut * (1 + P.f[1] * (.5 + vel / 254))), t);
    fl.frequency.setTargetAtTime(cut, t, P.f[3] * dMul / 2.5);
    fl.connect(amp); out = fl;
  }
  const oscs = [];
  const mkOsc = det => {
    const o = ctx.createOscillator(); o.type = P.o; o.detune.value = det;
    if (slideFrom) { o.frequency.setValueAtTime(440 * Math.pow(2, (slideFrom - 69) / 12), t); o.frequency.exponentialRampToValueAtTime(f, t + .07); }
    else o.frequency.setValueAtTime(f, t);
    o.connect(out); o.start(t); o.stop(stop); oscs.push(o); return o;
  };
  const o1 = mkOsc(0);
  if (P.o2) mkOsc(P.o2);
  const fm = P.fm || (m.weird > .05 ? [1.5, 0, .3] : null);
  if (fm) {
    const mo = ctx.createOscillator(), mg = ctx.createGain();
    mo.frequency.setValueAtTime(f * fm[0] * (1 + m.weird * .37), t);
    const idx = (fm[1] * Math.pow(2, (m.bright - .5) * 2) + m.weird * 3) * f;
    mg.gain.setValueAtTime(idx, t); mg.gain.setTargetAtTime(idx * .25, t, fm[2] * dMul / 2);
    mo.connect(mg); for (const o of oscs) mg.connect(o.frequency);
    mo.start(t); mo.stop(stop);
  }
  if (P.vib) {
    const lf = ctx.createOscillator(), lg = ctx.createGain(); lf.frequency.value = P.vib; lg.gain.setValueAtTime(0, t); lg.gain.linearRampToValueAtTime(f * .012, t + .25);
    lf.connect(lg); lg.connect(o1.frequency); lf.start(t); lf.stop(stop);
  }
  amp.connect(tin[ti]);
};

function noise(t, dur, type, freq, q) {
  const ctx = A.ctx, s = ctx.createBufferSource(), f = ctx.createBiquadFilter(), g = ctx.createGain();
  s.buffer = noiseBuf; s.loop = true; f.type = type; f.frequency.value = freq; f.Q.value = q || .7;
  s.connect(f); f.connect(g); s.start(t, Math.random()); s.stop(t + dur + .05);
  return { g, f };
}
function tone(t, type, f0, f1, sweep, dur, peak, dest) {
  const ctx = A.ctx, o = ctx.createOscillator(), g = ctx.createGain(); o.type = type;
  o.frequency.setValueAtTime(f0, t); o.frequency.exponentialRampToValueAtTime(Math.max(20, f1), t + sweep);
  g.gain.setValueAtTime(peak, t); g.gain.exponentialRampToValueAtTime(.001, t + dur);
  o.connect(g); g.connect(dest); o.start(t); o.stop(t + dur + .02);
}
A.drum = function (ti, kit, name, vel, t, mac) {
  if (!A.ready) return;
  const K = KITS[kit] || KITS['808'], m = mac || { bright: .5, punch: .5 }, v = vel / 127, out = tin[ti];
  const dm = Math.pow(2, (.5 - m.punch) * 2), bm = Math.pow(2, (m.bright - .5) * 2);
  const nz = (dur, type, freq, peak, q) => { const n = noise(t, dur, type, freq, q); n.g.gain.setValueAtTime(peak, t); n.g.gain.exponentialRampToValueAtTime(.001, t + dur); n.g.connect(out); return n; };
  switch (name) {
    case 'kick': tone(t, K.chip ? 'triangle' : 'sine', K.kf * K.ks, K.kf, .05, K.kd * dm, v * .95, out); if (!K.chip) nz(.012, 'highpass', 2000, v * .25); break;
    case 'snare': nz(K.sd * dm, 'highpass', 1400 * bm, v * .5); if (K.st) tone(t, 'triangle', K.st * 1.5, K.st, .03, .09, v * .4, out); break;
    case 'clap': { const n = noise(t, .25, 'bandpass', 1100 * bm, 1.2); n.g.gain.setValueAtTime(0, t);
      for (let i = 0; i < 3; i++) { n.g.gain.setValueAtTime(v * .6, t + i * .011); n.g.gain.exponentialRampToValueAtTime(.05, t + i * .011 + .009); }
      n.g.gain.setValueAtTime(v * .55, t + .033); n.g.gain.exponentialRampToValueAtTime(.001, t + .033 + .16 * dm); n.g.connect(out); break; }
    case 'chh': nz(K.hd * dm, 'highpass', K.hf * bm, v * .28); break;
    case 'ohh': nz(.22 * dm, 'highpass', K.hf * bm, v * .26); break;
    case 'rim': tone(t, 'square', 1700, 1650, .01, .025, v * .3, out); nz(.01, 'highpass', 3000, v * .2); break;
    case 'shaker': { const n = noise(t, .07, 'highpass', 5200 * bm); n.g.gain.setValueAtTime(0, t); n.g.gain.linearRampToValueAtTime(v * .16, t + .012); n.g.gain.exponentialRampToValueAtTime(.001, t + .06 * dm); n.g.connect(out); break; }
    case 'tomlo': tone(t, 'sine', 150, 85, .08, .22 * dm, v * .6, out); break;
    case 'tomhi': tone(t, 'sine', 240, 150, .07, .18 * dm, v * .55, out); break;
    default: { const ctx = A.ctx, bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 800; bp.connect(out);
      tone(t, 'square', 540, 540, .01, .14 * dm, v * .2, bp); tone(t, 'square', 800, 800, .01, .14 * dm, v * .2, bp); }
  }
};
A.fx = function (ti, kind, t, dur) {
  if (!A.ready) return;
  const out = tin[ti];
  if (kind === 'impact') {
    tone(t, 'sine', 120, 38, .25, 1.1, .8, out);
    const c = noise(t, 1.4, 'highpass', 4000); c.g.gain.setValueAtTime(.3, t); c.g.gain.exponentialRampToValueAtTime(.001, t + 1.3); c.g.connect(out);
  } else if (kind === 'riser') {
    const n = noise(t, dur, 'bandpass', 300, 2.5);
    n.f.frequency.setValueAtTime(300, t); n.f.frequency.exponentialRampToValueAtTime(7000, t + dur);
    n.g.gain.setValueAtTime(.001, t); n.g.gain.exponentialRampToValueAtTime(.45, t + dur * .95); n.g.gain.setTargetAtTime(0, t + dur, .01); n.g.connect(out);
  } else if (kind === 'pew') tone(t, 'square', 1900, 180, .22, .26, .25, out);
  else if (kind === 'ding') { tone(t, 'sine', 1318, 1318, .01, .5, .3, out); tone(t + .09, 'sine', 1976, 1976, .01, .7, .3, out); }
  else tone(t, 'sine', 1400, 700, .08, .2, .25, out);
};
A.PRESETS = PRESETS; A.KITS = KITS;
root.Sound = A;
})(window);
