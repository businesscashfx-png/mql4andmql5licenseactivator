/* app.js - lógica de la interfaz BCFX Licencia Activator */
'use strict';

(function () {
  const $ = (id) => document.getElementById(id);

  let eaSource = '';
  let eaName = '';
  let eaExt = '';
  let lastResult = null;   // { f, source, injected }

  let brokerChoice = 'one';
  let accChoice = 'all';

  // ------------------- utilidades -------------------
  function toast(msg) {
    const t = $('toast');
    t.textContent = msg;
    t.hidden = false;
    clearTimeout(t._h);
    t._h = setTimeout(() => { t.hidden = true; }, 2200);
  }

  function download(filename, content, mime) {
    const blob = new Blob([content], { type: mime || 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }

  async function copyText(text) {
    try {
      await navigator.clipboard.writeText(text);
      toast('Copiado al portapapeles');
    } catch (e) {
      // fallback
      const ta = document.createElement('textarea');
      ta.value = text; document.body.appendChild(ta); ta.select();
      try { document.execCommand('copy'); toast('Copiado al portapapeles'); }
      catch (e2) { toast('No se pudo copiar (copia manual)'); }
      document.body.removeChild(ta);
    }
  }

  function fmtDate(ts) {
    if (!ts || ts >= 0xffffffff) return '—';
    const d = new Date(ts * 1000);
    return d.toLocaleDateString('es-ES', { day: '2-digit', month: 'short', year: 'numeric' }) +
      ' ' + d.toLocaleTimeString('es-ES', { hour: '2-digit', minute: '2-digit' });
  }

  function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, (c) => ({
      '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    }[c]));
  }

  // ------------------- paso 1 : archivo -------------------
  const drop = $('drop');
  const fileInput = $('fileInput');

  drop.addEventListener('click', () => fileInput.click());
  fileInput.addEventListener('change', (e) => { if (e.target.files[0]) loadFile(e.target.files[0]); });
  $('fileMeta') && ($('fileMeta').innerHTML = '');

  ['dragover', 'dragenter'].forEach((ev) =>
    drop.addEventListener(ev, (e) => { e.preventDefault(); drop.classList.add('drag'); }));
  ['dragleave', 'drop'].forEach((ev) =>
    drop.addEventListener(ev, (e) => { e.preventDefault(); drop.classList.remove('drag'); }));

  drop.addEventListener('drop', (e) => {
    const f = e.dataTransfer.files && e.dataTransfer.files[0];
    if (f) loadFile(f);
  });

  function loadFile(file) {
    const name = (file.name || '').toLowerCase();
    if (!name.endsWith('.mq4') && !name.endsWith('.mq5')) {
      toast('Solo archivos .mq4 o .mq5');
      return;
    }
    const reader = new FileReader();
    reader.onload = () => {
      eaSource = String(reader.result || '');
      if (eaSource.indexOf('\u0000') >= 0 || !eaSource.trim()) {
        toast('El archivo parece binario o vacío. Usa un fuente .mq4/.mq5');
        return;
      }
      eaName = file.name;
      eaExt = name.endsWith('.mq4') ? 'mq4' : 'mq5';
      $('fileMeta').hidden = false;
      $('fileMeta').innerHTML =
        '<span class="tag ' + eaExt + '">' + eaExt.toUpperCase() + '</span>' +
        '<span>' + escapeHtml(file.name) + '</span>' +
        '<span class="muted">' + eaSource.length + ' caracteres</span>';
      $('addSystem').disabled = false;
      $('fileHint').textContent = 'Listo. Pulsa “Agregar el sistema”.';
      toast('Archivo cargado: ' + file.name);
    };
    reader.onerror = () => toast('No se pudo leer el archivo');
    reader.readAsText(file);
  }

  $('addSystem').addEventListener('click', () => {
    if (!eaSource) return;
    if ($('verbatim').checked) {
      if (!/(OnInit|OnTick)\s*\(/.test(eaSource)) {
        toast('Aviso: no se encontró OnInit/OnTick; el bloque se inserta igualmente (comprueba luego).');
      }
    }
    $('card-lic').hidden = false;
    $('card-lic').scrollIntoView({ behavior: 'smooth', block: 'start' });
    updateSummary();
  });

  $('resetFile').addEventListener('click', () => {
    $('card-lic').hidden = true;
    $('card-out').hidden = true;
    window.scrollTo({ top: 0, behavior: 'smooth' });
  });

  // ------------------- paso 2 : opciones -------------------
  let mode = 'U';

  const modeBtns = document.querySelectorAll('[data-mode]');
  modeBtns.forEach((b) => b.addEventListener('click', () => {
    modeBtns.forEach((x) => x.classList.remove('active'));
    b.classList.add('active');
    mode = b.dataset.mode;
    $('durRow').hidden = (mode !== 'T');
    updateSummary();
  }));

  const brokerBtns = document.querySelectorAll('[data-broker]');
  brokerBtns.forEach((b) => b.addEventListener('click', () => {
    brokerBtns.forEach((x) => x.classList.remove('active'));
    b.classList.add('active');
    brokerChoice = b.dataset.broker;
    $('broker').disabled = (brokerChoice === 'any');
    updateSummary();
  }));

  const accBtns = document.querySelectorAll('[data-acc]');
  accBtns.forEach((b) => b.addEventListener('click', () => {
    accBtns.forEach((x) => x.classList.remove('active'));
    b.classList.add('active');
    accChoice = b.dataset.acc;
    $('account').disabled = (accChoice === 'all');
    updateSummary();
  }));

  ['licensee', 'durVal', 'until', 'broker', 'account'].forEach((id) =>
    $(id).addEventListener('input', updateSummary));

  function updateSummary() {
    const name = $('licensee').value.trim() || '—';
    let type, duration;
    if (mode === 'U') { type = '∞ Ilimitada'; duration = 'Sin límites'; }
    else {
      type = '⏱ Por tiempo';
      const u = $('durUnit').value;
      const v = parseInt($('durVal').value, 10) || 0;
      const until = $('until').value;
      if (until) duration = 'Hasta ' + until;
      else if (u === 'h') duration = v + ' horas';
      else if (u === 'm') duration = v + ' meses (~' + (v * 30) + ' días)';
      else if (u === 'y') duration = v + ' años (~' + (v * 365) + ' días)';
      else duration = v + ' días';
    }
    const broker = brokerChoice === 'any' ? 'Cualquier broker (ilimitado)' : ($('broker').value.trim() || '—');
    const account = accChoice === 'all' ? 'Todas las cuentas' : ($('account').value.trim() || '—');

    $('summaryGrid').innerHTML =
      item('Persona', name) +
      item('Tipo', type, 'limit') +
      item('Vigencia', duration, 'time') +
      item('Broker', broker) +
      item('Cuenta', account);
  }
  function item(k, v, cls) {
    return '<div class="sum-item"><div class="k">' + k + '</div><div class="v ' + (cls || '') + '">' + escapeHtml(v) + '</div></div>';
  }

  // ------------------- generación -------------------
  $('generate').addEventListener('click', () => {
    const name = $('licensee').value.trim();
    if (!name) { toast('Escribe el nombre de la persona'); $('licensee').focus(); return; }

    let opts = { mode, licensee: name };

    if (mode === 'T') {
      const until = $('until').value;
      if (until) {
        const ts = Date.parse(until);
        if (isNaN(ts)) { toast('Fecha límite no válida'); return; }
        opts.until = until + 'T23:59:59Z';
      } else {
        const v = parseFloat($('durVal').value);
        if (!v || v <= 0) { toast('Indica un tiempo válido'); return; }
        const u = $('durUnit').value;
        const perDay = { h: 1 / 24, d: 1, m: 30, y: 365 }[u];
        opts.until = new Date(Date.now() + v * perDay * 86400 * 1000).toISOString();
      }
    }

    if (brokerChoice === 'one') {
      const b = $('broker').value.trim();
      if (!b) { toast('Escribe el nombre del broker'); $('broker').focus(); return; }
      opts.broker = b;
    } else {
      opts.broker = '**';
    }

    if (accChoice === 'one') {
      const a = $('account').value.trim();
      if (!a || isNaN(Number(a))) { toast('Número de cuenta no válido'); $('account').focus(); return; }
      opts.account = a;
    } else {
      opts.account = '0';
    }

    try {
      const f = window.Bcfx.genLicense(opts);

      // comprobación de rango temporal
      if (f.mode === 'T') {
        const now = Math.floor(Date.now() / 1000);
        if (f.exp <= now) { toast('La vigencia ya venció. Elige una fecha futura.'); return; }
        if (f.exp >= 0xffffffff) { toast('Fecha demasiado lejana (máx. año 2106).'); return; }
      }

      const verbatim = $('verbatim').checked;
      let source;
      if (verbatim) {
        source = window.Bcfx.inject(eaSource, f, f.lic).source;
      } else {
        source = window.Bcfx.inject(eaSource, f).source; // verificador opcional para vincular
      }

      lastResult = { f, source, injected: true };

      $('licText').textContent = f.lic;
      $('licFileText').textContent = f.lic;

      const chips = [];
      chips.push('<span class="chip">👤 ' + escapeHtml(f.licensee) + '</span>');
      if (f.mode === 'U') chips.push('<span class="chip">∞ Ilimitada</span>');
      else chips.push('<span class="chip">⏱ Expira ' + fmtDate(f.exp) + '</span>');
      chips.push('<span class="chip">🏦 ' + escapeHtml(f.broker === '**' ? 'Cualquier broker' : f.broker) + '</span>');
      chips.push('<span class="chip">🔢 ' + escapeHtml(f.account === '0' ? 'Todas las cuentas' : f.account) + '</span>');
      chips.push('<span class="chip">🧂 salt ' + escapeHtml(f.salt) + '</span>');
      $('licMeta').innerHTML = chips.join('');

      $('previewSrc').textContent = source;
      $('previewInfo').textContent = eaName + ' · ' + source.split('\n').length + ' líneas';

      $('card-out').hidden = false;
      $('card-out').scrollIntoView({ behavior: 'smooth', block: 'start' });
      $('genHint').textContent = '';
      toast('Licencia generada y EA protegido');
    } catch (err) {
      toast('Error: ' + err.message);
      $('genHint').textContent = 'Error: ' + err.message;
    }
  });

  // ------------------- salidas -------------------
  $('copyLic').addEventListener('click', () => lastResult && copyText(lastResult.f.lic));
  $('copyLicFile').addEventListener('click', () => lastResult && copyText(lastResult.f.lic));
  $('copySource').addEventListener('click', () => lastResult && copyText(lastResult.source));

  $('downloadEA').addEventListener('click', () => {
    if (!lastResult) return;
    const base = (eaName || 'EA_protegido').replace(/\.(mq4|mq5)$/i, '');
    download(base + '_PROTEGIDO.' + eaExt, lastResult.source, 'text/plain');
    toast('EA protegido descargado');
    const f = lastResult.f;
    download(base + '_LICENCIA.lic', f.lic, 'text/plain');
  });

  $('downloadLic').addEventListener('click', () => {
    if (!lastResult) return;
    const base = (eaName || 'EA_protegido').replace(/\.(mq4|mq5)$/i, '');
    download(base + '_LICENCIA.lic', lastResult.f.lic, 'text/plain');
    toast('Licencia .lic descargada');
  });

  // inicial
  updateSummary();
})();
