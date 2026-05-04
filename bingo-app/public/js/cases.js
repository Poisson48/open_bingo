import { state } from './state.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

export function renderCases() {
  const container = document.getElementById('cases-panel');
  container.innerHTML = `
    <div class="cases-section">

      <!-- ── Cases ── -->
      <div class="cases-header">
        <h2>Phrases</h2>
        <button id="add-case" class="btn-secondary">+ Ajouter une phrase</button>
      </div>
      <div class="cases-stats" id="cases-stats"></div>
      <div class="table-wrapper">
        <table id="cases-table">
          <thead>
            <tr>
              <th style="width:32px">#</th>
              <th>Libellé</th>
              <th style="width:64px" title="Points marqués quand la case est cochée">Pts</th>
              <th style="width:80px" title="Probabilité que cette phrase apparaisse dans la grille de chaque joueur">Taux&nbsp;%</th>
              <th style="width:36px"></th>
            </tr>
          </thead>
          <tbody id="cases-tbody">
            ${state.cases.map((c, i) => renderCaseRow(c, i)).join('')}
          </tbody>
        </table>
      </div>

      <!-- ── Gages ── -->
      <div class="cases-header" style="margin-top:36px">
        <h2>Gages <span class="badge-hint">récupèrent des PV</span></h2>
        <button id="add-gage" class="btn-secondary">+ Ajouter un gage</button>
      </div>
      <p class="hint" style="margin-bottom:12px">
        Les gages sont indépendants des phrases. Un joueur peut accomplir n'importe quel gage pour récupérer des PV.
      </p>
      <div class="table-wrapper">
        <table id="gages-table">
          <thead>
            <tr>
              <th style="width:32px">#</th>
              <th>Description du gage</th>
              <th style="width:72px" title="PV récupérés en accomplissant ce gage">+PV</th>
              <th style="width:36px"></th>
            </tr>
          </thead>
          <tbody id="gages-tbody">
            ${state.gages.map((g, i) => renderGageRow(g, i)).join('')}
          </tbody>
        </table>
      </div>

    </div>
  `;

  updateStats();

  // Cases events
  bindCaseTableEvents();
  document.getElementById('add-case').addEventListener('click', () => {
    state.cases.push({ label: '', points: 1, rate: 50 });
    const tbody = document.getElementById('cases-tbody');
    const i = state.cases.length - 1;
    const tr = document.createElement('tr');
    tr.dataset.idx = i;
    tr.innerHTML = renderCaseRowInner(state.cases[i], i);
    tbody.appendChild(tr);
    tr.querySelector('.case-label')?.focus();
    updateStats();
    bindCaseRowEvents(tr);
  });

  // Gages events
  bindGageTableEvents();
  document.getElementById('add-gage').addEventListener('click', () => {
    state.gages.push({ description: '', hp: 2 });
    const tbody = document.getElementById('gages-tbody');
    const i = state.gages.length - 1;
    const tr = document.createElement('tr');
    tr.dataset.idx = i;
    tr.innerHTML = renderGageRowInner(state.gages[i], i);
    tbody.appendChild(tr);
    tr.querySelector('.gage-desc')?.focus();
    bindGageRowEvents(tr);
  });
}

// ── Cases ──

function renderCaseRow(c, i) {
  return `<tr data-idx="${i}">${renderCaseRowInner(c, i)}</tr>`;
}

function renderCaseRowInner(c, i) {
  return `
    <td class="row-num">${i + 1}</td>
    <td><input type="text"   class="case-label"  value="${esc(c.label)}" placeholder="Ce qui se passe dans le film…"></td>
    <td><input type="number" class="case-points" value="${c.points}" min="1" max="99"></td>
    <td><input type="number" class="case-rate"   value="${c.rate ?? 50}" min="0" max="100"></td>
    <td><button class="btn-icon remove-case" data-idx="${i}" title="Supprimer">✕</button></td>
  `;
}

function bindCaseTableEvents() {
  document.querySelectorAll('#cases-tbody tr').forEach(tr => bindCaseRowEvents(tr));
}

function bindCaseRowEvents(tr) {
  tr.querySelector('.remove-case')?.addEventListener('click', () => {
    state.cases.splice(parseInt(tr.dataset.idx), 1);
    renderCases();
  });

  const sync = (sel, field, parse) => {
    tr.querySelector(sel)?.addEventListener('input', (e) => {
      const idx = parseInt(tr.dataset.idx);
      if (state.cases[idx]) { state.cases[idx][field] = parse(e.target.value); updateStats(); }
    });
  };

  sync('.case-label',  'label',  v => v);
  sync('.case-points', 'points', v => Math.max(1, parseInt(v) || 1));
  sync('.case-rate',   'rate',   v => Math.min(100, Math.max(0, parseInt(v) || 0)));
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
  document.querySelectorAll('#gages-tbody tr').forEach(tr => bindGageRowEvents(tr));
}

function bindGageRowEvents(tr) {
  tr.querySelector('.remove-gage')?.addEventListener('click', () => {
    state.gages.splice(parseInt(tr.dataset.idx), 1);
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
    statusMsg = `<span class="warn-mark">Aucune phrase — ajoutez au moins une.</span>`;
  } else if (count < available) {
    const diff = available - count;
    statusClass = 'stats-warn';
    statusMsg = `<span class="warn-mark">⚠ ${diff} phrase${diff > 1 ? 's' : ''} de moins que la taille de grille — des phrases se répéteront.</span>`;
  } else {
    const extra = count - available;
    statusClass = 'stats-ok';
    statusMsg = `<span class="ok-mark">✓ ${extra} phrase${extra !== 1 ? 's' : ''} de plus que la taille de grille.</span>`;
  }

  el.className = `cases-stats ${statusClass}`;
  el.innerHTML = `
    <span><strong>${count}</strong> phrase${count !== 1 ? 's' : ''}</span>
    <span class="sep">·</span>
    <span>Grille ${N}×${N} = ${available} cellule${available !== 1 ? 's' : ''}</span>
    <span class="sep">·</span>
    ${statusMsg}
  `;
}
