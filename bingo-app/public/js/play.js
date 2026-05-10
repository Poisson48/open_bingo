import { state } from './state.js';

// ── Wake Lock ─────────────────────────────────────────────────────────────────
let _wakeLock  = null;
let _playActive = false;

async function _acquireWakeLock() {
  if (!('wakeLock' in navigator) || _wakeLock) return;
  try {
    _wakeLock = await navigator.wakeLock.request('screen');
    _wakeLock.addEventListener('release', () => { _wakeLock = null; });
  } catch {}
}

async function _releaseWakeLock() {
  if (_wakeLock) { await _wakeLock.release(); _wakeLock = null; }
}

// Re-acquire when the page comes back to the foreground (e.g. after phone unlock)
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && _playActive) _acquireWakeLock();
});

export async function activatePlay()   { _playActive = true;  await _acquireWakeLock(); }
export async function deactivatePlay() { _playActive = false; await _releaseWakeLock(); closeFullscreen(); }

// ── Mode plein écran ─────────────────────────────────────────────────────────

let _fsGageTimer = null;
let _fsOverlay   = null;
let _fsCtx       = null; // { grid, checks, size, mid }

function openFullscreen(grid, checks, size) {
  closeFullscreen();
  _fsCtx = { grid, checks, size, mid: Math.floor(size / 2) };

  const overlay = document.createElement('div');
  overlay.id = 'play-fullscreen';
  overlay.innerHTML = `
    <div class="fs-header">
      <span class="fs-player">${esc(grid.player)}</span>
      <button class="fs-close">✕ Quitter</button>
    </div>
    <div class="fs-body">
      <div class="fs-grid-wrap">
        <table class="play-grid fs-grid" style="--grid-size:${size}">
          <tbody>${_buildFsRows()}</tbody>
        </table>
      </div>
      <div class="fs-gage-overlay" id="fs-gage-section"></div>
    </div>
  `;

  document.body.appendChild(overlay);
  _fsOverlay = overlay;

  overlay.querySelectorAll('.play-cell:not(.play-free)').forEach(td =>
    td.addEventListener('click', () => _onFsCell(td))
  );
  overlay.querySelector('.fs-close').addEventListener('click', closeFullscreen);

  try {
    const el = document.documentElement;
    if (el.requestFullscreen) {
      el.requestFullscreen().catch(() => {}).then(() => {
        if (screen.orientation?.lock) screen.orientation.lock('landscape').catch(() => {});
      });
    } else if (el.webkitRequestFullscreen) {
      el.webkitRequestFullscreen();
      if (screen.orientation?.lock) screen.orientation.lock('landscape').catch(() => {});
    }
  } catch {}
}

function _buildFsRows() {
  const { grid, checks, size, mid } = _fsCtx;
  const bingoSet = _bingoSet(checks, size);
  return grid.cells.map((row, r) =>
    `<tr>${row.map((cell, c) => {
      const isFree  = cell.free || (state.freeCenter && size % 2 === 1 && r === mid && c === mid);
      const checked = checks[r]?.[c] ?? false;
      const inBingo = bingoSet.has(`${r},${c}`);
      let cls = 'play-cell';
      if (isFree)       cls += ' play-free';
      else if (inBingo) cls += ' play-checked play-bingo';
      else if (checked) cls += ' play-checked';
      return `<td class="${cls}" data-row="${r}" data-col="${c}">
        <div class="play-cell-inner">
          <span class="play-cell-label">${esc(isFree ? 'FREE' : (cell.label || ''))}</span>
          ${checked && !isFree ? '<div class="play-check-mark">✓</div>' : ''}
        </div>
      </td>`;
    }).join('')}</tr>`
  ).join('');
}

function _buildFsGage(gageData, newCombos = []) {
  const combos = state.comboGages || {};
  const COMBO_META = {
    line:     { icon: '↔', label: 'Ligne complète' },
    column:   { icon: '↕', label: 'Colonne complète' },
    diagonal: { icon: '⤡', label: 'Diagonale complète' },
  };
  const comboHtml = newCombos
    .filter(key => combos[key])
    .map(key => {
      const { icon, label } = COMBO_META[key];
      return `<div class="fs-combo-item">
        <div class="play-gage-card-header">
          <span class="play-gage-num">${icon} ${label}</span>
        </div>
        <div class="play-gage-desc fs-gage-desc">${esc(combos[key])}</div>
      </div>`;
    }).join('');

  if (!gageData && !comboHtml) return '';
  return `<div class="fs-gage-active">
    ${gageData ? `
    <div class="play-gage-card-header">
      <span class="play-gage-num">Gage #${gageData.num}</span>
      <span class="play-gage-label">${esc(gageData.label)}</span>
    </div>
    <div class="play-gage-desc fs-gage-desc">${esc(gageData.desc)}</div>` : ''}
    ${comboHtml}
    <div class="fs-gage-timer"><div class="fs-gage-timer-bar"></div></div>
  </div>`;
}

function _bingoSet(checks, size) {
  const s = new Set();
  detectBingo(checks, size).forEach(line => line.forEach(([r, c]) => s.add(`${r},${c}`)));
  return s;
}

function _onFsCell(cellEl) {
  const { grid, checks, size, mid } = _fsCtx;
  const r = +cellEl.dataset.row, c = +cellEl.dataset.col;

  // Snapshot combo types before toggle
  const oldTypes = new Set(
    detectBingo(checks, size).map(line => detectLineType(line, size)).filter(Boolean)
  );

  checks[r][c] = !checks[r][c];
  saveChecks(grid.player, checks);

  // Update cell appearances
  const bingoSet = _bingoSet(checks, size);
  _fsOverlay.querySelectorAll('.play-cell:not(.play-free)').forEach(td => {
    const tr = +td.dataset.row, tc = +td.dataset.col;
    const chk = checks[tr]?.[tc];
    td.className = 'play-cell' +
      (bingoSet.has(`${tr},${tc}`) ? ' play-checked play-bingo' : chk ? ' play-checked' : '');
    const mark = td.querySelector('.play-check-mark');
    if (chk && !mark) td.querySelector('.play-cell-inner').insertAdjacentHTML('beforeend', '<div class="play-check-mark">✓</div>');
    else if (!chk && mark) mark.remove();
  });

  // Detect newly completed combo types (only when checking)
  const newCombos = [];
  if (checks[r][c]) {
    const newTypes = new Set(
      detectBingo(checks, size).map(line => detectLineType(line, size)).filter(Boolean)
    );
    newTypes.forEach(t => { if (!oldTypes.has(t)) newCombos.push(t); });
  }

  // Cell gage
  if (_fsGageTimer) { clearTimeout(_fsGageTimer); _fsGageTimer = null; }
  let gageData = null;
  if (checks[r][c] && state.gageMode) {
    const cell = grid.cells[r]?.[c];
    const gage = cell?.points ? state.gages?.[cell.points - 1] : null;
    if (gage) gageData = { num: cell.points, label: cell.label, desc: gage.description };
  }

  const sec = document.getElementById('fs-gage-section');
  if (sec) {
    sec.innerHTML = _buildFsGage(gageData, newCombos);
    if (gageData || newCombos.length > 0) {
      _fsGageTimer = setTimeout(() => {
        const s2 = document.getElementById('fs-gage-section');
        if (s2) s2.innerHTML = _buildFsGage(null, []);
      }, 15000);
    }
  }
}

function closeFullscreen() {
  if (_fsGageTimer)  { clearTimeout(_fsGageTimer); _fsGageTimer = null; }
  if (_fsOverlay)    { _fsOverlay.remove(); _fsOverlay = null; }
  _fsCtx = null;
  try {
    if (document.fullscreenElement)            document.exitFullscreen();
    else if (document.webkitFullscreenElement) document.webkitExitFullscreen();
  } catch {}
  try { if (screen.orientation?.unlock) screen.orientation.unlock(); } catch {}
  renderPlay();
}

const PLAY_NS = 'bingo_play';

function playKey(projectId, playerName) {
  return `${PLAY_NS}_${projectId}_${encodeURIComponent(playerName)}`;
}

function loadChecks(playerName) {
  try {
    const raw = localStorage.getItem(playKey(state.id, playerName));
    return raw ? JSON.parse(raw) : null;
  } catch { return null; }
}

function saveChecks(playerName, checks) {
  localStorage.setItem(playKey(state.id, playerName), JSON.stringify(checks));
}

function emptyChecks(size) {
  return Array.from({ length: size }, () => Array(size).fill(false));
}

let _selectedPlayerIdx = 0;
let _lastProjectId = null;
let _lastCheckedCell = null; // { r, c } — gage mode: shows gage card

export function renderPlay() {
  const panel = document.getElementById('play-panel');

  if (_lastProjectId !== state.id) {
    _selectedPlayerIdx = 0;
    _lastProjectId = state.id;
    _lastCheckedCell = null;
  }

  if (!state.grids || state.grids.length === 0) {
    panel.innerHTML = `
      <div class="empty-state">
        <span class="empty-icon">🎱</span>
        <strong>Aucune grille générée</strong>
        Allez dans l'onglet <strong>Grilles</strong> pour générer les grilles d'abord.
      </div>`;
    return;
  }

  const players = state.grids.map(g => g.player);
  if (_selectedPlayerIdx >= players.length) _selectedPlayerIdx = 0;

  const grid = state.grids[_selectedPlayerIdx];
  const size = state.gridSize;
  const mid  = Math.floor(size / 2);

  let checks = loadChecks(grid.player);
  if (!checks || checks.length !== size || !checks[0] || checks[0].length !== size) {
    checks = emptyChecks(size);
    if (state.freeCenter && size % 2 === 1) checks[mid][mid] = true;
    saveChecks(grid.player, checks);
  }

  const completedLines  = detectBingo(checks, size);
  const score           = computeScore(grid, checks, size);
  const checkedCount    = checks.flat().filter(Boolean).length;
  const bingoCount      = completedLines.length;

  const bingoSet = new Set();
  completedLines.forEach(line => line.forEach(([r, c]) => bingoSet.add(`${r},${c}`)));

  const playerOptions = players.map((p, i) =>
    `<option value="${i}"${i === _selectedPlayerIdx ? ' selected' : ''}>${esc(p)}</option>`
  ).join('');

  const rows = grid.cells.map((row, r) =>
    `<tr>${row.map((cell, c) => {
      const isFree   = cell.free || (state.freeCenter && size % 2 === 1 && r === mid && c === mid);
      const checked  = checks[r]?.[c] ?? false;
      const inBingo  = bingoSet.has(`${r},${c}`);
      const isActive = _lastCheckedCell && _lastCheckedCell.r === r && _lastCheckedCell.c === c;
      let cls = 'play-cell';
      if (isFree) cls += ' play-free';
      else if (inBingo) cls += ' play-checked play-bingo';
      else if (checked) cls += ' play-checked';
      if (isActive && state.gageMode && !isFree) cls += ' play-cell-active';
      return `<td class="${cls}" data-row="${r}" data-col="${c}">
        <div class="play-cell-inner">
          <span class="play-cell-label">${esc(isFree ? 'FREE' : (cell.label || ''))}</span>
          ${!isFree ? renderCellBadge(cell) : ''}
          ${checked && !isFree ? `<div class="play-check-mark">✓</div>` : ''}
        </div>
      </td>`;
    }).join('')}</tr>`
  ).join('');

  panel.innerHTML = `
    <div class="play-container">
      <div class="play-top">
        <div class="play-player-row">
          <span class="play-player-label">Joueur</span>
          <select id="play-player-select" class="play-player-select">${playerOptions}</select>
        </div>
        <div class="play-score-bar">
          <div class="play-score-item">
            <span class="play-score-num">${score}</span>
            <span class="play-score-sub">${state.gageMode ? 'gages' : 'pts'}</span>
          </div>
          <div class="play-score-item">
            <span class="play-score-num play-score-cases">${checkedCount}/${size * size}</span>
            <span class="play-score-sub">cases</span>
          </div>
          ${bingoCount > 0 ? `
          <div class="play-bingo-badge">
            🎉 ${bingoCount} BINGO${bingoCount > 1 ? 'S' : ''}
          </div>` : ''}
        </div>
      </div>

      ${state.gageMode ? renderGageActionCard(grid, checks) : ''}

      <div class="play-fullscreen-row">
        <button id="play-start-fs" class="btn-primary">
          ⛶ Commencer la partie
        </button>
      </div>

      <div class="play-rotate-hint">
        ↻ Tourne ton téléphone en paysage pour mieux voir la grille
      </div>

      <div class="play-grid-wrap">
        <table class="play-grid" style="--grid-size:${size}">
          <tbody>${rows}</tbody>
        </table>
      </div>

      ${renderRulesSection(completedLines)}

      <div class="play-footer">
        <button id="play-reset" class="btn-secondary">↺ Réinitialiser</button>
      </div>
    </div>`;

  // Events
  panel.querySelector('#play-player-select').addEventListener('change', e => {
    _selectedPlayerIdx = +e.target.value;
    _lastCheckedCell = null;
    renderPlay();
  });

  panel.querySelectorAll('.play-cell:not(.play-free)').forEach(cell => {
    cell.addEventListener('click', () => {
      const r = +cell.dataset.row;
      const c = +cell.dataset.col;
      const wasChecked = checks[r][c];
      checks[r][c] = !wasChecked;
      if (state.gageMode && checks[r][c]) {
        _lastCheckedCell = { r, c };
      } else if (state.gageMode && !checks[r][c] && _lastCheckedCell?.r === r && _lastCheckedCell?.c === c) {
        _lastCheckedCell = null;
      }
      saveChecks(grid.player, checks);
      renderPlay();
    });
  });

  panel.querySelector('#play-start-fs').addEventListener('click', () => {
    openFullscreen(grid, checks, size);
  });

  panel.querySelector('#play-reset').addEventListener('click', () => {
    if (!confirm('Réinitialiser toutes les cases cochées ?')) return;
    checks = emptyChecks(size);
    if (state.freeCenter && size % 2 === 1) checks[mid][mid] = true;
    _lastCheckedCell = null;
    saveChecks(grid.player, checks);
    renderPlay();
  });
}

// ─── Cell badge (pts or gage ref) ────────────────────────────────────────────

function renderCellBadge(cell) {
  if (!cell.points) return '';
  if (state.gageMode) {
    return `<span class="play-cell-pts play-gage-ref">#${cell.points}</span>`;
  }
  return `<span class="play-cell-pts">${cell.points}pts</span>`;
}

// ─── Gage action card (mode gage uniquement) ─────────────────────────────────

function renderGageActionCard(grid, checks) {
  if (!_lastCheckedCell) {
    return `<div class="play-gage-card play-gage-card-hint">
      <span class="play-gage-icon">👆</span>
      <span>Coche une case pour voir le gage à effectuer</span>
    </div>`;
  }
  const { r, c } = _lastCheckedCell;
  const cell = grid.cells[r]?.[c];
  if (!cell || !cell.points) return '';
  const gageIdx = cell.points - 1;
  const gage    = state.gages?.[gageIdx];
  const isUnchecked = !(checks[r]?.[c]);
  if (!gage || isUnchecked) return '';
  return `<div class="play-gage-card play-gage-card-active">
    <div class="play-gage-card-header">
      <span class="play-gage-num">Gage #${cell.points}</span>
      <span class="play-gage-label">${esc(cell.label)}</span>
    </div>
    <div class="play-gage-desc">${esc(gage.description)}</div>
  </div>`;
}

// ─── Rules / Combo section ────────────────────────────────────────────────────

function renderRulesSection(completedLines) {
  if (state.gageMode) {
    return renderComboGagesSection(completedLines);
  }

  const m = state.multipliers || {};
  return `
    <div class="play-rules">
      <div class="play-rules-title">Règles des combinaisons</div>
      <table class="play-rules-table">
        <tr><td>Ligne complète</td><td>valeur <span class="play-rules-mult">× ${m.line ?? 2}</span></td></tr>
        <tr><td>Colonne complète</td><td>valeur <span class="play-rules-mult">× ${m.column ?? 2}</span></td></tr>
        <tr><td>Diagonale complète</td><td>valeur <span class="play-rules-mult">× ${m.diagonal ?? 3}</span></td></tr>
        <tr><td>Grille complète (BINGO)</td><td>valeur <span class="play-rules-mult">× ${m.full ?? 10}</span></td></tr>
      </table>
      <p class="play-rules-note">
        Cochez une case quand l'événement se produit. La valeur en points est dans le coin bas-droit de chaque case.
      </p>
    </div>`;
}

function renderComboGagesSection(completedLines) {
  const combos = state.comboGages || {};
  if (!combos.line && !combos.column && !combos.diagonal) return '';

  // Determine which combo types are completed
  const completedTypes = new Set();
  const size = state.gridSize;
  completedLines.forEach(line => {
    const type = detectLineType(line, size);
    if (type) completedTypes.add(type);
  });

  const items = [
    { key: 'line',     label: 'Ligne complète',     icon: '↔' },
    { key: 'column',   label: 'Colonne complète',   icon: '↕' },
    { key: 'diagonal', label: 'Diagonale complète', icon: '⤡' },
  ]
    .filter(({ key }) => combos[key])
    .map(({ key, label, icon }) => {
      const done = completedTypes.has(key);
      return `<div class="play-combo-item${done ? ' play-combo-done' : ''}">
        <div class="play-combo-header">
          <span class="play-combo-icon">${icon}</span>
          <span class="play-combo-label">${label}</span>
          ${done ? `<span class="play-combo-check">✓ Déclenché !</span>` : ''}
        </div>
        <div class="play-combo-desc">${esc(combos[key])}</div>
      </div>`;
    }).join('');

  if (!items) return '';

  return `
    <div class="play-rules play-combos">
      <div class="play-rules-title">Gages de Combinaison</div>
      ${items}
      <p class="play-rules-note">
        Réalise le gage correspondant dès que tu complètes la combinaison !
      </p>
    </div>`;
}

function detectLineType(line, size) {
  if (!line.length) return null;
  const rows = line.map(([r]) => r);
  const cols = line.map(([, c]) => c);
  if (new Set(rows).size === 1) return 'line';
  if (new Set(cols).size === 1) return 'column';
  // diagonal: all r===c or all r+c===size-1
  if (line.every(([r, c]) => r === c) || line.every(([r, c]) => r + c === size - 1)) return 'diagonal';
  return null;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

function esc(str) {
  return String(str ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function computeScore(grid, checks, size) {
  let score = 0;
  for (let r = 0; r < size; r++) {
    for (let c = 0; c < size; c++) {
      if (checks[r]?.[c] && grid.cells[r]?.[c]) {
        score += grid.cells[r][c].points || 0;
      }
    }
  }
  return score;
}

function detectBingo(checks, size) {
  const lines = [];
  for (let r = 0; r < size; r++) {
    if (checks[r]?.every(Boolean)) lines.push(Array.from({ length: size }, (_, c) => [r, c]));
  }
  for (let c = 0; c < size; c++) {
    if (Array.from({ length: size }, (_, r) => checks[r]?.[c]).every(Boolean))
      lines.push(Array.from({ length: size }, (_, r) => [r, c]));
  }
  if (Array.from({ length: size }, (_, i) => checks[i]?.[i]).every(Boolean))
    lines.push(Array.from({ length: size }, (_, i) => [i, i]));
  if (Array.from({ length: size }, (_, i) => checks[i]?.[size - 1 - i]).every(Boolean))
    lines.push(Array.from({ length: size }, (_, i) => [i, size - 1 - i]));
  return lines;
}
