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
  const isLocal = loc.hostname === 'localhost' || loc.hostname === '127.0.0.1' || loc.hostname === '' || loc.hostname.endsWith('tauri.localhost');
  const root = isLocal
    ? 'https://poisson48.github.io/open_bingo/'
    : loc.href.split('#')[0].replace(/\/[^/]*$/, '/');
  return root + 'landing.html#s=' + encoded;
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

// ── Scanner QR natif (Android WebView) ───────────────────────────────────────

/**
 * Ouvre un overlay plein écran avec un flux caméra et utilise BarcodeDetector
 * pour détecter les QR codes en temps réel.
 * Retourne une Promise qui résout avec l'URL scannée, ou null si annulé.
 */
export function openQrScanner() {
  return new Promise((resolve) => {
    // Vérification de BarcodeDetector (disponible dans Android WebView >= API 27 / Chrome 83+)
    const hasBarcodeDetector = typeof BarcodeDetector !== 'undefined';

    const overlay = document.createElement('div');
    overlay.className = 'qr-scanner-overlay';

    if (!hasBarcodeDetector) {
      overlay.innerHTML = `
        <div class="qr-scanner-error">
          Le scanner QR n'est pas disponible sur cet appareil.<br>
          Utilisez le lien de partage à la place.
        </div>
        <button class="qr-scanner-cancel">Fermer</button>
      `;
      document.body.appendChild(overlay);
      overlay.querySelector('.qr-scanner-cancel').addEventListener('click', () => {
        overlay.remove();
        resolve(null);
      });
      return;
    }

    overlay.innerHTML = `
      <video class="qr-scanner-video" playsinline autoplay muted></video>
      <div class="qr-scanner-frame"></div>
      <p class="qr-scanner-hint">Pointez la caméra vers un QR code Open Bingo</p>
      <button class="qr-scanner-cancel">Annuler</button>
    `;
    document.body.appendChild(overlay);

    const video = overlay.querySelector('.qr-scanner-video');
    let stream = null;
    let rafId = null;
    let closed = false;

    const cleanup = () => {
      closed = true;
      if (rafId) cancelAnimationFrame(rafId);
      if (stream) stream.getTracks().forEach(t => t.stop());
      overlay.remove();
    };

    overlay.querySelector('.qr-scanner-cancel').addEventListener('click', () => {
      cleanup();
      resolve(null);
    });

    const startScan = async () => {
      try {
        stream = await navigator.mediaDevices.getUserMedia({
          video: { facingMode: 'environment', width: { ideal: 1280 }, height: { ideal: 720 } }
        });
        if (closed) { stream.getTracks().forEach(t => t.stop()); return; }
        video.srcObject = stream;
        await video.play();

        const detector = new BarcodeDetector({ formats: ['qr_code'] });

        const scan = async () => {
          if (closed) return;
          if (video.readyState >= 2) {
            try {
              const barcodes = await detector.detect(video);
              if (barcodes.length > 0) {
                const raw = barcodes[0].rawValue;
                cleanup();
                resolve(raw);
                return;
              }
            } catch { /* frame skipped */ }
          }
          rafId = requestAnimationFrame(scan);
        };
        rafId = requestAnimationFrame(scan);
      } catch (err) {
        if (closed) return;
        const hint = overlay.querySelector('.qr-scanner-hint');
        if (hint) hint.textContent = 'Impossible d\'accéder à la caméra. Vérifiez les permissions.';
      }
    };

    // Sur Android WebView, demander la permission via le pont natif si disponible,
    // puis lancer la caméra (getUserMedia déclenchera onPermissionRequest dans RustWebChromeClient)
    if (window.AndroidQr) {
      window.AndroidQr.requestCameraPermission('window.__qrCameraPermissionCb');
      window.__qrCameraPermissionCb = (granted) => {
        delete window.__qrCameraPermissionCb;
        if (granted) {
          startScan();
        } else {
          const hint = overlay.querySelector('.qr-scanner-hint');
          if (hint) hint.textContent = 'Permission caméra refusée.';
        }
      };
    } else {
      // Navigateur standard : getUserMedia gérera la demande de permission
      startScan();
    }
  });
}

/**
 * Résout un lien court (is.gd) vers l'URL finale.
 * Retourne l'URL d'origine en cas d'échec.
 */
async function resolveShortUrl(url) {
  // is.gd fournit une API de résolution
  try {
    if (url.includes('is.gd/')) {
      const slug = url.split('is.gd/').pop().replace(/\/$/, '');
      const resp = await fetch(
        `https://is.gd/forward.php?format=json&shorturl=${encodeURIComponent(slug)}`,
        { signal: AbortSignal.timeout(5000) }
      );
      if (resp.ok) {
        const data = await resp.json();
        if (data.url) return data.url;
      }
    }
  } catch { /* ignore */ }
  return url;
}

/**
 * Traite une URL scannée : résout les liens courts, extrait le hash #s=…
 * et retourne les données du projet ou null.
 */
export async function importFromScannedUrl(rawUrl) {
  if (!rawUrl) return null;

  // Tenter de résoudre un lien court
  let url = rawUrl.trim();
  const resolved = await resolveShortUrl(url);
  url = resolved;

  // Extraire le fragment #s=... de l'URL
  const hashIdx = url.indexOf('#');
  if (hashIdx === -1) return null;
  const hash = url.slice(hashIdx);
  if (!hash.includes('s=')) return null;

  return parseShareHash(hash);
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

export async function openShareModal(projectData, { onImport } = {}) {
  let longUrl;
  try { longUrl = await buildShareUrl(projectData); }
  catch { return; }

  // Vérifier si le scanner QR est disponible (Android WebView avec AndroidQr bridge)
  const scannerAvailable = typeof window.AndroidQr !== 'undefined' && window.AndroidQr.isAvailable();

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
      ${scannerAvailable ? `<button class="btn-secondary btn-scan-qr" id="btn-scan-qr">📷 Scanner un QR code</button>` : ''}
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

  // ── Scanner QR (Android uniquement) ──────────────────────────────────────
  if (scannerAvailable) {
    overlay.querySelector('#btn-scan-qr').addEventListener('click', async () => {
      const rawUrl = await openQrScanner();
      if (!rawUrl) return;
      const data = await importFromScannedUrl(rawUrl);
      if (data && typeof onImport === 'function') {
        close();
        onImport(data);
      } else if (!data) {
        // QR scanné mais pas un lien open_bingo valide
        const hint = overlay.querySelector('.hint');
        if (hint) {
          hint.textContent = 'QR code non reconnu. Assurez-vous qu\'il s\'agit d\'un lien Open Bingo.';
          hint.style.color = 'var(--red, #f87171)';
        }
      }
    });
  }

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
