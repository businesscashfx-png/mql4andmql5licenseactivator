/* BCFX License Activator - servidor estático mínimo (sin dependencias)
   Uso: node server.js  ->  http://0.0.0.0:3000  (o process.env.PORT)
*/
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = __dirname;
const PORT = process.env.PORT || 3000;

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.htm':  'text/html; charset=utf-8',
  '.js':   'text/javascript; charset=utf-8',
  '.mjs':  'text/javascript; charset=utf-8',
  '.css':  'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.txt':  'text/plain; charset=utf-8',
  '.md':   'text/plain; charset=utf-8',
  '.lic':  'text/plain; charset=utf-8',
  '.svg':  'image/svg+xml',
  '.png':  'image/png',
  '.ico':  'image/x-icon',
  '.webp': 'image/webp',
  '.mq4':  'application/octet-stream',
  '.mq5':  'application/octet-stream',
  '.ex4':  'application/octet-stream',
  '.ex5':  'application/octet-stream',
  '.exe':  'application/x-msdownload',
  '.ico':  'image/x-icon',
  '.rc':   'text/plain; charset=utf-8',
  '.c':    'text/plain; charset=utf-8',
};

const server = http.createServer((req, res) => {
  let urlPath;
  try {
    urlPath = decodeURIComponent(new URL(req.url, 'http://localhost').pathname);
  } catch (e) {
    res.writeHead(400); res.end('Bad request');
    return;
  }

  if (urlPath === '/' || urlPath === '') urlPath = '/index.html';

  const filePath = path.normalize(path.join(ROOT, urlPath));
  if (!filePath.startsWith(ROOT)) { // evita path traversal
    res.writeHead(403); res.end('Forbidden');
    return;
  }

  fs.stat(filePath, (err, st) => {
    if (err || !st.isFile()) {
      // si es el .exe, un error claro
      res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
      res.end('No encontrado: ' + urlPath);
      return;
    }
    const ext = path.extname(filePath).toLowerCase();
    const headers = {
      'Content-Type': MIME[ext] || 'application/octet-stream',
      'Content-Length': st.size,
      'Cache-Control': 'no-cache',
    };
    if (ext === '.exe') {
      headers['Content-Disposition'] = 'attachment; filename="BCFX_Activador_Windows.exe"';
    }
    res.writeHead(200, headers);
    fs.createReadStream(filePath).pipe(res);
  });
});

server.listen(PORT, '0.0.0.0', () => {
  console.log('BCFX License Activator corriendo en http://0.0.0.0:' + PORT);
});
