/* crypto.js - SHA-256 y HMAC-SHA256 en JavaScript puro (sin dependencias).
   IDÉNTICO en semántica al SHA-256 de MQL4/MQL5 (WinCrypt), para que:
     - la firma se calcule en el navegador (herramienta web)
     - y se verifique dentro del EA (MQL4/MQL5) con el mismo resultado.
*/
'use strict';

const K = new Uint32Array([
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
]);

function rotr(x, n) { return (x >>> n) | (x << (32 - n)); }

function sha256Bytes(bytes) {
  const len = bytes.length;
  const bitLenHi = Math.floor(len / 0x20000000);
  const bitLenLo = (len << 3) >>> 0;
  // bloques = partida_entera((len + 8) / 64) + 1  (idéntico a la fórmula del EA)
  const padded = new Uint8Array(((((len + 8) / 64) | 0) + 1) * 64);
  padded.set(bytes);
  padded[len] = 0x80;
  const dv = new DataView(padded.buffer);
  dv.setUint32(padded.length - 8, bitLenHi);
  dv.setUint32(padded.length - 4, bitLenLo);

  let h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
  let h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;
  const w = new Uint32Array(64);

  for (let off = 0; off < padded.length; off += 64) {
    for (let i = 0; i < 16; i++) {
      w[i] = dv.getUint32(off + i * 4);
    }
    for (let i = 16; i < 64; i++) {
      const s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >>> 3);
      const s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >>> 10);
      w[i] = (w[i - 16] + s0 + w[i - 7] + s1) >>> 0;
    }
    let a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
    for (let i = 0; i < 64; i++) {
      const S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const ch = (e & f) ^ (~e & g);
      const t1 = (h + S1 + ch + K[i] + w[i]) >>> 0;
      const S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const maj = (a & b) ^ (a & c) ^ (b & c);
      const t2 = (S0 + maj) >>> 0;
      h = g; g = f; f = e;
      e = (d + t1) >>> 0;
      d = c; c = b; b = a;
      a = (t1 + t2) >>> 0;
    }
    h0 = (h0 + a) >>> 0; h1 = (h1 + b) >>> 0; h2 = (h2 + c) >>> 0; h3 = (h3 + d) >>> 0;
    h4 = (h4 + e) >>> 0; h5 = (h5 + f) >>> 0; h6 = (h6 + g) >>> 0; h7 = (h7 + h) >>> 0;
  }
  return [h0, h1, h2, h3, h4, h5, h6, h7];
}

function bytesToHex(words) {
  let out = '';
  for (const w of words) {
    out += ('00000000' + (w >>> 0).toString(16)).slice(-8);
  }
  return out;
}

function utf8Bytes(str) {
  if (typeof TextEncoder !== 'undefined') return new TextEncoder().encode(str);
  if (typeof Buffer !== 'undefined') return new Uint8Array(Buffer.from(str, 'utf8'));
  // fallback puro (QuickJS embebido en el .exe, etc.)
  const bytes = [];
  for (let i = 0; i < str.length; i++) {
    let code = str.codePointAt(i);
    if (code > 0xffff) i++;           // salta el par sustituto bajo
    if (code < 0x80) bytes.push(code);
    else if (code < 0x800) {
      bytes.push(0xc0 | (code >> 6));
      bytes.push(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
      bytes.push(0xe0 | (code >> 12));
      bytes.push(0x80 | ((code >> 6) & 0x3f));
      bytes.push(0x80 | (code & 0x3f));
    } else {
      bytes.push(0xf0 | (code >> 18));
      bytes.push(0x80 | ((code >> 12) & 0x3f));
      bytes.push(0x80 | ((code >> 6) & 0x3f));
      bytes.push(0x80 | (code & 0x3f));
    }
  }
  return new Uint8Array(bytes);
}

/** Hex SHA-256 de un string (UTF-8). Debe coincidir con mqlSHA256() del EA. */
function sha256Hex(str) {
  return bytesToHex(sha256Bytes(utf8Bytes(str)));
}

function xorBytes(a, b) {
  const out = new Uint8Array(a.length);
  for (let i = 0; i < a.length; i++) out[i] = a[i] ^ b[i];
  return out;
}

/**
 * HMAC-SHA256 sobre datos binarios, clave como string UTF-8.
 * Devuelve array de 8 words (igual que CryptEncode CRYPT_HASH_SHA256 en MQL).
 */
function hmacSha256Words(keyStr, messageBytes) {
  const blockSize = 64;
  let keyBytes = utf8Bytes(keyStr);
  if (keyBytes.length > blockSize) {
    keyBytes = new Uint8Array(wordsToBytes(sha256Bytes(keyBytes)));
  }
  const key = new Uint8Array(blockSize);
  key.set(keyBytes);

  const ipad = new Uint8Array(blockSize);
  const opad = new Uint8Array(blockSize);
  for (let i = 0; i < blockSize; i++) { ipad[i] = 0x36; opad[i] = 0x5c; }

  const inner = new Uint8Array(blockSize + messageBytes.length);
  inner.set(xorBytes(key, ipad)); inner.set(messageBytes, blockSize);
  const innerHash = sha256Bytes(inner);

  const outer = new Uint8Array(blockSize + 32);
  outer.set(xorBytes(key, opad)); outer.set(wordsToBytes(innerHash), blockSize);
  return sha256Bytes(outer);
}

function wordsToBytes(words) {
  const out = new Uint8Array(words.length * 4);
  for (let i = 0; i < words.length; i++) {
    out[i * 4]     = (words[i] >>> 24) & 0xff;
    out[i * 4 + 1] = (words[i] >>> 16) & 0xff;
    out[i * 4 + 2] = (words[i] >>> 8)  & 0xff;
    out[i * 4 + 3] =  words[i]         & 0xff;
  }
  return out;
}

/** HMAC-SHA256 en hex. Debe coincidir con mqlHMACSHA256() del EA. */
function hmacSha256Hex(keyStr, messageStr) {
  return bytesToHex(hmacSha256Words(keyStr, utf8Bytes(messageStr)));
}

const api = { sha256Hex, hmacSha256Hex, hmacSha256Words, bytesToHex, utf8Bytes, sha256Bytes };

if (typeof module !== 'undefined' && module.exports) {
  module.exports = api;
}
// expone el API globalmente (navegador, QuickJS embebido en el .exe, etc.)
if (typeof globalThis !== 'undefined') {
  globalThis.BcfxCrypto = api;
}
if (typeof self !== 'undefined' && self !== globalThis) {
  self.BcfxCrypto = api;
}
