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
  const base = window.location.href.split('#')[0];
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
  const url = await buildShareUrl(projectData);

  const overlay = document.createElement('div');
  overlay.className = 'modal-overlay';
  overlay.innerHTML = `
    <div class="modal-box share-modal">
      <h3>Partager la partie</h3>
      <div class="share-qr" id="share-qr-target"></div>
      <div class="share-url-row">
        <input class="share-url-input" type="text" readonly value="">
        <button class="btn-copy btn-secondary btn-sm" id="btn-copy-link">Copier</button>
      </div>
      <p class="hint">Scannez le QR code ou copiez le lien. Le destinataire recevra une copie du projet.</p>
      <div class="modal-actions">
        <button class="btn-secondary" id="btn-close-share">Fermer</button>
      </div>
    </div>
  `;

  // Afficher l'URL tronquée dans l'input pour la lisibilité, mais copier l'URL complète
  overlay.querySelector('.share-url-input').value = url;

  document.body.appendChild(overlay);

  // QR code
  const qrTarget = overlay.querySelector('#share-qr-target');
  if (window.QRCode) {
    new window.QRCode(qrTarget, {
      text: url,
      width: 220,
      height: 220,
      colorDark: '#f1f5f9',
      colorLight: '#1a2235',
      correctLevel: window.QRCode.CorrectLevel.M
    });
  } else {
    qrTarget.innerHTML = '<span class="hint" style="padding:16px;display:block;text-align:center">QR code indisponible hors ligne</span>';
  }

  // Copier
  const btnCopy = overlay.querySelector('#btn-copy-link');
  btnCopy.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(url);
    } catch {
      const inp = overlay.querySelector('.share-url-input');
      inp.select();
      document.execCommand('copy');
    }
    const orig = btnCopy.textContent;
    btnCopy.textContent = 'Copié !';
    setTimeout(() => { btnCopy.textContent = orig; }, 2000);
  });

  // Fermer
  const close = () => overlay.remove();
  overlay.querySelector('#btn-close-share').addEventListener('click', close);
  overlay.addEventListener('click', e => { if (e.target === overlay) close(); });
}
