/* tools/embed-js.js - Genera desktop/js_embed.h a partir de crypto.js y mql.js
   Uso: node tools/embed-js.js
*/
'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const crypto = fs.readFileSync(path.join(ROOT, 'crypto.js'), 'utf8');
const mql = fs.readFileSync(path.join(ROOT, 'mql.js'), 'utf8');

// Asegura final de línea y escapa a literal C
function cStr(js) {
  let out = '"';
  for (let i = 0; i < js.length; i++) {
    const c = js[i];
    switch (c) {
      case '\\': out += '\\\\'; break;
      case '"':  out += '\\"';  break;
      case '\n': out += '\\n"\n"'; break;
      case '\r': out += '\\r';  break;
      case '\t': out += '\\t';  break;
      default:
        if (c.charCodeAt(0) < 32) out += '\\x' + ('0' + c.charCodeAt(0).toString(16)).slice(-2);
        else out += c;
    }
  }
  out += '"';
  return out;
}

const hdr = `/* js_embed.h - AUTO-GENERADO por tools/embed-js.js. NO EDITAR. */
#ifndef BCFX_JS_EMBED_H
#define BCFX_JS_EMBED_H

static const char EMBED_CRYPTO_JS[] =
${cStr(crypto)};

static const size_t EMBED_CRYPTO_JS_LEN = sizeof(EMBED_CRYPTO_JS) - 1;

static const char EMBED_MQL_JS[] =
${cStr(mql)};

static const size_t EMBED_MQL_JS_LEN = sizeof(EMBED_MQL_JS) - 1;

#endif
`;

fs.writeFileSync(path.join(ROOT, 'desktop', 'js_embed.h'), hdr);
console.log('js_embed.h generado:', hdr.length, 'bytes');
