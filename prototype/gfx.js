/* Pocket Band graphics: a 240x240 indexed framebuffer (the FM-1's real screen size),
 * 8 colours per skin, a 5x7 bitmap font and 16x16 sprites. Indexed colour makes skins free. */
(function (root) {
'use strict';
const W = 240, H = 240, fb = new Uint8Array(W * H);

/* Colours: 0 paper, 1 dim, 2 mid, 3 ink, 4-6 accents, 7 highlight. */
const SKINS = [
  { name: 'MIDNIGHT', c: ['#0b1020', '#1f2a4d', '#4a5fa8', '#dfe7ff', '#ff5d8f', '#ffd166', '#4de0c0', '#ffffff'] },
  { name: 'PEA SOUP', c: ['#9bbc0f', '#8bac0f', '#306230', '#0f380f', '#306230', '#0f380f', '#306230', '#e8f8b0'] },
  { name: 'POCKET', c: ['#c4cfa1', '#a3ad82', '#4d533c', '#1f1f1f', '#4d533c', '#1f1f1f', '#6b7353', '#eef3d8'] },
  { name: 'GRAPE', c: ['#1b1030', '#3b2468', '#7a54c8', '#e8dcff', '#ffcf4a', '#58e0a0', '#ff6fb0', '#ffffff'] },
  { name: 'CHERRY', c: ['#2b0a14', '#5e1630', '#c0395a', '#ffd9d0', '#ffb347', '#7fe0d0', '#ff7aa8', '#ffffff'] },
  { name: 'AMBER CRT', c: ['#140a00', '#4a2800', '#b86a00', '#ffb000', '#ff8a00', '#ffd27a', '#c97a00', '#fff0c0'] },
  { name: 'ICE', c: ['#e8f4ff', '#b8d4ee', '#5a86b8', '#12284a', '#e0457b', '#e09a1a', '#1aa890', '#ffffff'] },
  { name: 'VAPOR', c: ['#1a0b2e', '#46237a', '#ff6ad5', '#c8f7ff', '#fff06a', '#00e5ff', '#8c6cff', '#ffffff'] },
  { name: 'BONE', c: ['#f4ecd8', '#d6c9a8', '#8a7a5c', '#2a2318', '#c0392b', '#2a7f9e', '#d08a1a', '#fffaf0'] }
];
const lut = new Uint32Array(8);
let skin = 0;
function setSkin(i) {
  skin = ((i % SKINS.length) + SKINS.length) % SKINS.length;
  SKINS[skin].c.forEach((h, k) => {
    const v = parseInt(h.slice(1), 16);
    lut[k] = 0xff000000 | ((v & 0xff) << 16) | (v & 0xff00) | (v >> 16);   /* ABGR for little-endian ImageData */
  });
}
setSkin(0);

const FONT = ('0000000000' + '00005F0000' + '0007000700' + '147F147F14' + '242A7F2A12' + '2313086462' + '3649552250' + '0005030000' +
  '001C224100' + '0041221C00' + '14083E0814' + '08083E0808' + '0050300000' + '0808080808' + '0060600000' + '2010080402' +
  '3E5149453E' + '00427F4000' + '4261514946' + '2141454B31' + '1814127F10' + '2745454539' + '3C4A494930' + '0171090503' +
  '3649494936' + '064949291E' + '0036360000' + '0056360000' + '0814224100' + '1414141414' + '0041221408' + '0201510906' +
  '324979413E' + '7E1111117E' + '7F49494936' + '3E41414122' + '7F4141221C' + '7F49494941' + '7F09090901' + '3E4149497A' +
  '7F0808087F' + '00417F4100' + '2040413F01' + '7F08142241' + '7F40404040' + '7F020C027F' + '7F0408107F' + '3E4141413E' +
  '7F09090906' + '3E4151215E' + '7F09192946' + '4649494931' + '01017F0101' + '3F4040403F' + '1F2040201F' + '3F4038403F' +
  '6314081463' + '0708700807' + '6151494543' + '007F414100' + '0204081020' + '0041417F00' + '0402010204' + '4040404040');
const glyph = new Uint8Array(64 * 5);
for (let i = 0; i < 320; i++) glyph[i] = parseInt(FONT.substr(i * 2, 2), 16);

function cls(c) { fb.fill(c); }
function px(x, y, c) { if (x >= 0 && y >= 0 && x < W && y < H) fb[(y | 0) * W + (x | 0)] = c; }
function rect(x, y, w, h, c) {
  x |= 0; y |= 0; w |= 0; h |= 0;
  const x0 = Math.max(0, x), y0 = Math.max(0, y), x1 = Math.min(W, x + w), y1 = Math.min(H, y + h);
  for (let j = y0; j < y1; j++) fb.fill(c, j * W + x0, j * W + Math.max(x0, x1));
}
function frame(x, y, w, h, c) { rect(x, y, w, 1, c); rect(x, y + h - 1, w, 1, c); rect(x, y, 1, h, c); rect(x + w - 1, y, 1, h, c); }
function line(x0, y0, x1, y1, c, dash) {
  x0 |= 0; y0 |= 0; x1 |= 0; y1 |= 0;
  const dx = Math.abs(x1 - x0), dy = -Math.abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  let err = dx + dy, n = 0;
  for (;;) {
    if (!dash || (n++ >> 1) % 2 === 0) px(x0, y0, c);
    if (x0 === x1 && y0 === y1) break;
    const e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
function text(x, y, s, c, scale, bg) {
  s = String(s).toUpperCase(); scale = scale || 1; x |= 0; y |= 0;
  if (bg !== undefined) rect(x - 1, y - 1, s.length * 6 * scale + 1, 9 * scale, bg);
  for (let i = 0; i < s.length; i++) {
    let ch = s.charCodeAt(i) - 32; if (ch < 0 || ch > 63) ch = 31;
    for (let col = 0; col < 5; col++) {
      const bits = glyph[ch * 5 + col];
      for (let r = 0; r < 7; r++) if (bits & (1 << r)) {
        if (scale === 1) px(x + i * 6 + col, y + r, c); else rect(x + (i * 6 + col) * scale, y + r * scale, scale, scale, c);
      }
    }
  }
}
function textC(cx, y, s, c, scale, bg) { text(cx - (String(s).length * 6 * (scale || 1) - (scale || 1)) / 2, y, s, c, scale, bg); }
function textR(rx, y, s, c, scale) { text(rx - String(s).length * 6 * (scale || 1) + (scale || 1), y, s, c, scale); }
function bar(x, y, w, h, v, c, back) { rect(x, y, w, h, back === undefined ? 1 : back); rect(x, y, Math.round(w * Math.max(0, Math.min(1, v))), h, c); }

/* Sprites: k outline, b body (the member's own colour), w highlight, d shade, e eye. */
const ART = [
  ['......k..k......', '......k..k......', '...kkkkkkkkkk...', '..kbbbbbbbbbbk..', '..kbwwbbbbwwbk..', '..kbwebbbbewbk..', '..kbbbbbbbbbbk..', '..kbbkkkkkkbbk..',
   '..kbbbbbbbbbbk..', '...kkkkkkkkkk...', '..kkdbbbbbbdkk..', '.kbkdbbbbbbdkbk.', '.kkkdbbbbbbdkkk.', '....kkkkkkkk....', '....kbk..kbk....', '....kkk..kkk....'],
  ['................', '......kkkk......', '.....kbbbbk.....', '....kbbbbbbk....', '...kbbebbebbk...', '...kbbbbbbbbkkk.', '..kkbbbwwbbbwwk.', '.kbbbbbbbbbbkkk.',
   '.kbbbbbbbbbbk...', '.kdbbbbbbbbbk...', '..kdbbbbbbbk....', '...kddbbbdk.....', '....kkkkkk......', '.....k..k.......', '....kk..kk......', '................'],
  ['................', '................', '.....kkkkkk.....', '...kkbbbbbbkk...', '..kbbbbbbbbbbk..', '.kbbwbbbbbbbbbk.', '.kbbbbbbbbbbbbk.', 'kbbbebbbbbbebbbk',
   'kbbbebbbbbbebbbk', 'kbbbbbbkkbbbbbbk', 'kbbbbbbbbbbbbbbk', 'kdbbbbbbbbbbbbdk', '.kddbbbbbbbbddk.', '..kkddddddddkk..', '....kkkkkkkk....', '................'],
  ['..k........k....', '.kbk......kbk...', '.kbbk....kbbk...', '.kbbbkkkkbbbk...', '.kbbbbbbbbbbk...', '.kbbebbbbebbk...', '.kbbebbbbebbk...', '.kbbbbwwbbbbk...',
   '.kbbbbbbbbbbk.k.', '..kbbbbbbbbk.kbk', '..kbbbbbbbbkkbk.', '.kbbbbbbbbbbbk..', '.kbdbbbbbbdbk...', '.kbdbkkkkbdbk...', '.kkkk....kkkk...', '................'],
  ['................', '.....kkkkkk.....', '...kkbbbbbbkk...', '..kbbbbbbbbbbk..', '.kbbwbbbbbbbbbk.', '.kbbbbbbbbbbbbk.', '.kbbebbbbbbebbk.', '.kbbebbbbbbebbk.',
   '.kbbbbbbbbbbbbk.', '.kbbbbbkkbbbbbk.', '.kbbbbbbbbbbbbk.', '.kbbbbbbbbbbbbk.', '.kdbbbbbbbbbbdk.', '.kdbkdbbbkdbbdk.', '.kkk.kkkk.kkkk..', '................'],
  ['.......kk.......', '......kbbk......', '......kbbk......', '.kkkkkbbbbkkkkk.', '.kbbbbbbbbbbbbk.', '..kbbebbbbebbk..', '...kbebbbbebk...', '...kbbbwwbbbk...',
   '..kbbbbbbbbbbk..', '..kbbbbbbbbbbk..', '.kbbbbbkkbbbbbk.', '.kbbbkk..kkbbbk.', '.kbkk......kkbk.', '.kk..........kk.', '................', '................'],
  ['...kk....kk.....', '..kbbk..kbbk....', '..kbbk..kbbk....', '..kbbk..kbbk....', '..kbbkkkkbbk....', '.kbbbbbbbbbbk...', '.kbbebbbbebbk...', '.kbbebbbbebbk...',
   '.kbbbbwwbbbbk...', '.kbbbbbbbbbbk...', '..kbbbbbbbbk....', '.kbbbbbbbbbbk...', '.kbbbbbbbbbbkkk.', '.kdbbbbbbbbdkbk.', '..kkkk..kkkkkk..', '................'],
  ['.......k........', '......kwk.......', '.......k........', '.....kkkkkk.....', '....kbbbbbbk....', '...kbbebbebbk...', '...kbbebbebbk...', '.kkkkkkkkkkkkkk.',
   'kddwddwddwddwddk', '.kkkkkkkkkkkkkk.', '...k..k..k..k...', '..k...k..k...k..', '................', '................', '................', '................']
];
const BODY = [4, 5, 6, 5, 6, 4, 5, 6];
/* opt: {sleep, squash, body (override colour), dim} */
function sprite(i, x, y, scale, opt) {
  const a = ART[i % 8]; opt = opt || {}; x |= 0; y |= 0;
  const body = opt.dim ? 2 : (opt.body !== undefined ? opt.body : BODY[i % 8]);
  for (let r = 0; r < 16; r++) {
    const row = a[r], yy = y + (opt.squash && r < 8 ? r + 1 : r) * scale;
    for (let c = 0; c < 16; c++) {
      const ch = row[c]; if (!ch || ch === '.') continue;
      let col;
      if (ch === 'k') col = 3; else if (ch === 'b') col = body; else if (ch === 'w') col = opt.dim ? 1 : 7; else if (ch === 'd') col = opt.dim ? 1 : 2;
      else col = opt.sleep ? ((r & 1) ? 3 : body) : 3;
      if (scale === 1) px(x + c, yy, col); else rect(x + c * scale, yy, scale, scale, col);
    }
  }
}

let ctx2d = null, img = null, buf32 = null;
function attach(canvas) {
  ctx2d = canvas.getContext('2d'); img = ctx2d.createImageData(W, H); buf32 = new Uint32Array(img.data.buffer);
}
function present() {
  if (!ctx2d) return;
  for (let i = 0; i < W * H; i++) buf32[i] = lut[fb[i]];
  ctx2d.putImageData(img, 0, 0);
}
root.G = { W, H, fb, SKINS, setSkin, skin: () => skin, cls, px, rect, frame, line, text, textC, textR, bar, sprite, BODY, attach, present };
})(window);
