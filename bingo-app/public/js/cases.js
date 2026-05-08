import { state, scheduleAutoSave, markGridsDirty } from './state.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

function rateClass(r) {
  if (r === 0) return 'rate-zero';
  if (r < 50)  return 'rate-low';
  return 'rate-high';
}

export function renderCases() {
  const container = document.getElementById('cases-panel');

  // Mise à jour badge onglet
  document.querySelector('[data-tab="cases"]')?.setAttribute('data-count', state.cases.length || '');

  container.innerHTML = `
    <div class="cases-section">

      <!-- ── Cases ── -->
      <div class="cases-header">
        <h2>Phrases <span class="badge">${state.cases.length}</span></h2>
        <button id="add-case" class="btn-secondary">+ Ajouter</button>
      </div>
      <div class="cases-stats" id="cases-stats"></div>
      <div class="table-wrapper">
        <table id="cases-table">
          <thead>
            <tr>
              <th style="width:32px">#</th>
              <th>Libellé</th>
              <th style="width:64px" title="Points quand la case est cochée">Pts</th>
              <th style="width:160px" title="Probabilité d'apparition dans la grille">Taux</th>
              <th style="width:36px"></th>
            </tr>
          </thead>
          <tbody id="cases-tbody">
            ${state.cases.length
              ? state.cases.map((c, i) => renderCaseRow(c, i)).join('')
              : `<tr><td colspan="5" style="padding:0;border:none">
                  <div class="empty-state" style="border:none;border-radius:0;margin:0">
                    <span class="empty-icon">📝</span>
                    <strong>Aucune phrase</strong>
                    Ajoutez les événements qui apparaîtront dans les grilles.
                  </div>
                </td></tr>`
            }
          </tbody>
        </table>
      </div>

      <!-- ── Gages ── -->
      <div class="cases-header" style="margin-top:40px">
        <h2>Gages <span class="badge">${state.gages.length}</span> <span class="badge-hint">récupèrent des PV</span></h2>
        <button id="add-gage" class="btn-secondary">+ Ajouter</button>
      </div>
      <p class="hint" style="margin-bottom:14px">
        Indépendants des phrases — un joueur accomplit un gage pour récupérer des PV.
      </p>
      <div class="table-wrapper">
        <table id="gages-table">
          <thead>
            <tr>
              <th style="width:32px">#</th>
              <th>Description du gage</th>
              <th style="width:80px" title="PV récupérés">+PV</th>
              <th style="width:36px"></th>
            </tr>
          </thead>
          <tbody id="gages-tbody">
            ${state.gages.length
              ? state.gages.map((g, i) => renderGageRow(g, i)).join('')
              : `<tr><td colspan="4" style="padding:0;border:none">
                  <div class="empty-state" style="border:none;border-radius:0;margin:0">
                    <span class="empty-icon">⚡</span>
                    <strong>Aucun gage</strong>
                    Optionnel — ajoutez des défis pour récupérer des PV.
                  </div>
                </td></tr>`
            }
          </tbody>
        </table>
      </div>

    </div>
  `;

  updateStats();
  bindCaseTableEvents();
  bindGageTableEvents();
  // Initialiser le gradient des sliders
  document.querySelectorAll('.case-rate').forEach(el => {
    el.style.setProperty('--rate', el.value + '%');
  });

  document.getElementById('add-case').addEventListener('click', () => {
    state.cases.push({ label: '', points: 1, rate: 50 });
    markGridsDirty();
    scheduleAutoSave();
    renderCases();
    const rows = document.querySelectorAll('#cases-tbody tr');
    rows[rows.length - 1]?.querySelector('.case-label')?.focus({ preventScroll: true });
  });

  document.getElementById('add-gage').addEventListener('click', () => {
    state.gages.push({ description: '', hp: 2 });
    scheduleAutoSave();
    renderCases();
    const rows = document.querySelectorAll('#gages-tbody tr');
    rows[rows.length - 1]?.querySelector('.gage-desc')?.focus({ preventScroll: true });
  });
}

// ── Cases ──

function renderCaseRow(c, i) {
  const rate = c.rate ?? 50;
  return `<tr data-idx="${i}">${renderCaseRowInner(c, i)}</tr>`;
}

function renderCaseRowInner(c, i) {
  const rate = c.rate ?? 50;
  return `
    <td class="row-num">${i + 1}</td>
    <td><input type="text"   class="case-label"  value="${esc(c.label)}"  placeholder="Ce qui se passe…"></td>
    <td><input type="number" class="case-points" value="${c.points}" min="1" max="99"></td>
    <td class="rate-cell">
      <input type="range" class="case-rate" value="${rate}" min="0" max="100" step="10" style="--rate:${rate}%">
      <span class="rate-val ${rateClass(rate)}">${rate}%</span>
    </td>
    <td><button class="btn-icon remove-case" data-idx="${i}" title="Supprimer">✕</button></td>
  `;
}

function bindCaseTableEvents() {
  document.querySelectorAll('#cases-tbody tr[data-idx]').forEach(tr => bindCaseRowEvents(tr));
}

function bindCaseRowEvents(tr) {
  tr.querySelector('.remove-case')?.addEventListener('click', () => {
    state.cases.splice(parseInt(tr.dataset.idx), 1);
    markGridsDirty();
    scheduleAutoSave();
    renderCases();
  });

  tr.querySelector('.case-label')?.addEventListener('input', (e) => {
    const idx = parseInt(tr.dataset.idx);
    if (state.cases[idx]) { state.cases[idx].label = e.target.value; markGridsDirty(); }
  });

  tr.querySelector('.case-points')?.addEventListener('input', (e) => {
    const idx = parseInt(tr.dataset.idx);
    if (state.cases[idx]) { state.cases[idx].points = Math.max(1, parseInt(e.target.value) || 1); updateStats(); markGridsDirty(); }
  });

  tr.querySelector('.case-rate')?.addEventListener('input', (e) => {
    const idx = parseInt(tr.dataset.idx);
    const val = parseInt(e.target.value) || 0;
    e.target.style.setProperty('--rate', val + '%');
    const span = tr.querySelector('.rate-val');
    if (span) {
      span.textContent = val + '%';
      span.className   = 'rate-val ' + rateClass(val);
    }
    if (state.cases[idx]) { state.cases[idx].rate = val; updateStats(); markGridsDirty(); }
  });
}

// ── Gages ──

function renderGageRow(g, i) {
  return `<tr data-idx="${i}">${renderGageRowInner(g, i)}</tr>`;
}

function renderGageRowInner(g, i) {
  return `
    <td class="row-num">${i + 1}</td>
    <td><input type="text"   class="gage-desc" value="${esc(g.description)}" placeholder="Ce que le joueur doit faire…"></td>
    <td><input type="number" class="gage-hp"   value="${g.hp ?? 2}" min="0" max="99"></td>
    <td><button class="btn-icon remove-gage" data-idx="${i}" title="Supprimer">✕</button></td>
  `;
}

function bindGageTableEvents() {
  document.querySelectorAll('#gages-tbody tr[data-idx]').forEach(tr => bindGageRowEvents(tr));
}

function bindGageRowEvents(tr) {
  tr.querySelector('.remove-gage')?.addEventListener('click', () => {
    state.gages.splice(parseInt(tr.dataset.idx), 1);
    scheduleAutoSave();
    renderCases();
  });

  tr.querySelector('.gage-desc')?.addEventListener('input', (e) => {
    const idx = parseInt(tr.dataset.idx);
    if (state.gages[idx]) state.gages[idx].description = e.target.value;
  });

  tr.querySelector('.gage-hp')?.addEventListener('input', (e) => {
    const idx = parseInt(tr.dataset.idx);
    if (state.gages[idx]) state.gages[idx].hp = Math.max(0, parseInt(e.target.value) || 0);
  });
}

// ── Stats ──

function updateStats() {
  const el = document.getElementById('cases-stats');
  if (!el) return;

  const N = state.gridSize;
  const total = N * N;
  const hasCenter = state.freeCenter && N % 2 === 1;
  const available = hasCenter ? total - 1 : total;
  const count = state.cases.length;

  let statusClass, statusMsg;
  if (count === 0) {
    statusClass = 'stats-warn';
    statusMsg = `<span class="warn-mark">⚠ Aucune phrase — ajoutez-en au moins une.</span>`;
  } else if (count < available) {
    const diff = available - count;
    statusClass = 'stats-warn';
    statusMsg = `<span class="warn-mark">⚠ ${diff} phrase${diff > 1 ? 's' : ''} manquante${diff > 1 ? 's' : ''} — des doublons apparaîtront.</span>`;
  } else {
    const extra = count - available;
    statusClass = 'stats-ok';
    statusMsg = `<span class="ok-mark">✓ ${extra} phrase${extra !== 1 ? 's' : ''} de surplus — bonne variété !</span>`;
  }

  el.className = `cases-stats ${statusClass}`;
  el.innerHTML = `
    <strong>${count}</strong> phrase${count !== 1 ? 's' : ''}
    <span class="sep">·</span>
    Grille ${N}×${N} → ${available} cellule${available !== 1 ? 's' : ''}
    <span class="sep">·</span>
    ${statusMsg}
  `;
}
