// ── Compression (native CompressionStream, fallback base64 brut) ──────────────

async function compress(str) {
  if (typeof CompressionStream === 'undefined') {
    return 'b64:' + btoa(unescape(encodeURIComponent(str)));
  }
  const bytes = new TextEncoder().encode(str);
  const cs = new CompressionStream('deflate-raw');
  const w = cs.writable.getWriter();
  w.write(bytes);
  w.close();
  const chunks = [];
  const r = cs.readable.getReader();
  for (;;) { const { done, value } = await r.read(); if (done) break; chunks.push(value); }
  const out = new Uint8Array(chunks.reduce((n, c) => n + c.length, 0));
  let off = 0;
  for (const c of chunks) { out.set(c, off); off += c.length; }
  return 'z:' + btoa(String.fromCharCode(...out)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '');
}

async function decompress(encoded) {
  if (encoded.startsWith('b64:')) {
    return decodeURIComponent(escape(atob(encoded.slice(4))));
  }
  if (!encoded.startsWith('z:')) return null;
  const b64 = encoded.slice(2).replace(/-/g, '+').replace(/_/g, '/');
  const pad = b64 + '='.repeat((4 - b64.length % 4) % 4);
  const binary = atob(pad);
  const bytes = Uint8Array.from(binary, c => c.charCodeAt(0));
  const ds = new DecompressionStream('deflate-raw');
  const w = ds.writable.getWriter();
  w.write(bytes);
  w.close();
  const chunks = [];
  const r = ds.readable.getReader();
  for (;;) { const { done, value } = await r.read(); if (done) break; chunks.push(value); }
  const out = new Uint8Array(chunks.reduce((n, c) => n + c.length, 0));
  let off = 0;
  for (const c of chunks) { out.set(c, off); off += c.length; }
  return new TextDecoder().decode(out);
}

// ── Hash encode/decode ─────────────────────────────────────────────────────────

async function buildShareUrl(projectData) {
  const encoded = await compress(JSON.stringify(projectData));
  const loc = window.location;
  const isLocal = loc.hostname === 'localhost' || loc.hostname === '127.0.0.1' || loc.hostname === '';
  const base = isLocal ? 'https://poisson48.github.io/open_bingo/' : loc.href.split('#')[0];
  return base + '#s=' + encoded;
}

async function parseShareHash(hash) {
  const m = hash.match(/[#&]?s=(z:[A-Za-z0-9_-]+|b64:[A-Za-z0-9+/=]+)/);
  if (!m) return null;
  try {
    const json = await decompress(m[1]);
    return json ? JSON.parse(json) : null;
  } catch { return null; }
}

// ── URL shortener ─────────────────────────────────────────────────────────────

async function shortenUrl(url) {
  const resp = await fetch(
    `https://is.gd/create.php?format=json&url=${encodeURIComponent(url)}`,
    { signal: AbortSignal.timeout(5000) }
  );
  if (!resp.ok) throw new Error('HTTP ' + resp.status);
  const data = await resp.json();
  if (data.errorcode) throw new Error(data.errormessage);
  return data.shorturl;
}

// ── Clipboard ─────────────────────────────────────────────────────────────────

async function copyText(text, inputEl) {
  if (navigator.clipboard && window.isSecureContext) {
    try { await navigator.clipboard.writeText(text); return; } catch {}
  }
  if (inputEl) {
    inputEl.removeAttribute('readonly');
    inputEl.select();
    inputEl.setSelectionRange(0, 99999);
    document.execCommand('copy');
    inputEl.setAttribute('readonly', '');
    return;
  }
  const ta = document.createElement('textarea');
  ta.value = text;
  Object.assign(ta.style, { position: 'fixed', opacity: '0', top: '0', left: '0' });
  document.body.appendChild(ta);
  ta.focus();
  ta.select();
  document.execCommand('copy');
  ta.remove();
}

// ── QR code helper ────────────────────────────────────────────────────────────

function renderQR(container, url) {
  container.innerHTML = '';
  if (!window.QRCode) {
    container.innerHTML = '<span class="share-qr-msg">QR code indisponible</span>';
    return;
  }
  try {
    new window.QRCode(container, {
      text: url,
      width: 220,
      height: 220,
      colorDark: '#f1f5f9',
      colorLight: '#1a2235',
      correctLevel: window.QRCode.CorrectLevel.M
    });
  } catch {
    container.innerHTML = '<span class="share-qr-msg">URL trop longue pour un QR code — utilisez le lien.</span>';
  }
}

// ── Import au chargement ───────────────────────────────────────────────────────

export async function tryImportFromHash() {
  const hash = window.location.hash;
  if (!hash.includes('s=')) return null;
  const data = await parseShareHash(hash);
  if (!data) return null;
  history.replaceState(null, '', window.location.pathname + window.location.search);
  return data;
}

// ── Modal partage ─────────────────────────────────────────────────────────────

export async function openShareModal(projectData) {
  let longUrl;
  try { longUrl = await buildShareUrl(projectData); }
  catch { return; }

  const overlay = document.createElement('div');
  overlay.className = 'modal-overlay';
  overlay.innerHTML = `
    <div class="modal-box share-modal">
      <h3>Partager la partie</h3>
      <div class="share-qr" id="share-qr-target">
        <span class="share-qr-msg">Génération du QR code…</span>
      </div>
      <div class="share-url-row">
        <input class="share-url-input" type="text" readonly>
        <button class="btn-copy btn-secondary btn-sm" id="btn-copy-link">Copier</button>
      </div>
      <p class="hint">Scannez le QR code ou copiez le lien. Le destinataire recevra une copie du projet avec les grilles.</p>
      <div class="modal-actions">
        <button class="btn-secondary" id="btn-close-share">Fermer</button>
      </div>
    </div>
  `;

  const urlInput = overlay.querySelector('.share-url-input');
  urlInput.value = longUrl;
  document.body.appendChild(overlay);

  // ── Listeners en premier ──────────────────────────────────────────────────
  const close = () => overlay.remove();
  overlay.querySelector('#btn-close-share').addEventListener('click', close);
  overlay.addEventListener('click', e => { if (e.target === overlay) close(); });

  // currentUrl est muable : mis à jour si raccourcissement réussit
  let currentUrl = longUrl;
  const btnCopy = overlay.querySelector('#btn-copy-link');
  btnCopy.addEventListener('click', async () => {
    try { await copyText(currentUrl, urlInput); } catch {}
    btnCopy.textContent = 'Copié !';
    setTimeout(() => { btnCopy.textContent = 'Copier'; }, 2000);
  });

  // ── Raccourcissement + QR (asynchrone, après les listeners) ──────────────
  const qrTarget = overlay.querySelector('#share-qr-target');
  try {
    const shortUrl = await shortenUrl(longUrl);
    currentUrl = shortUrl;
    urlInput.value = shortUrl;
    renderQR(qrTarget, shortUrl);
  } catch {
    // Pas de réseau ou is.gd indisponible → QR avec l'URL longue en fallback
    renderQR(qrTarget, longUrl);
  }
}
