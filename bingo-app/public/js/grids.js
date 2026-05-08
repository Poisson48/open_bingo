import { state, scheduleAutoSave, gridsDirty } from './state.js';
import { generateAll, reshuffleGrid } from './generator.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

const AVATAR_COLORS = [
  '#6366f1','#8b5cf6','#ec4899','#f59e0b',
  '#10b981','#3b82f6','#f43f5e','#06b6d4'
];

function avatarColor(name) {
  let h = 0;
  for (let i = 0; i < name.length; i++) h = (h * 31 + name.charCodeAt(i)) | 0;
  return AVATAR_COLORS[Math.abs(h) % AVATAR_COLORS.length];
}

function initials(name) {
  const parts = (name || '?').trim().split(/\s+/);
  return parts.map(p => p[0] || '').join('').toUpperCase().slice(0, 2) || '?';
}

export function renderGrids() {
  const container = document.getElementById('grids-panel');

  // Badge onglet
  document.querySelector('[data-tab="grids"]')?.setAttribute('data-count', state.grids.length || '');

  let html = `
    <div class="grids-section">
      <div class="grids-header">
        <h2>Grilles générées <span class="badge">${state.grids.length}</span></h2>
        <button id="btn-generate-all" class="btn-primary">⟳ Générer les grilles</button>
      </div>
      <div id="grids-error"></div>
      ${gridsDirty && state.grids.length > 0
        ? `<div class="warn-banner">⚠ Les phrases ont changé depuis la dernière génération — les grilles peuvent être obsolètes.</div>`
        : ''}
  `;

  if (state.grids.length === 0) {
    html += `
      <div class="empty-state">
        <span class="empty-icon">🎲</span>
        <strong>Aucune grille générée</strong>
        Configurez les joueurs et les phrases, puis cliquez sur "Générer les grilles".
      </div>`;
  } else {
    html += `<div class="grids-list">`;
    state.grids.forEach((grid, i) => {
      const color = avatarColor(grid.player);
      html += `
        <div class="grid-card">
          <div class="grid-card-header">
            <div class="player-info">
              <div class="player-avatar" style="background:${color}">${esc(initials(grid.player))}</div>
              <span class="grid-player-name">${esc(grid.player)}</span>
            </div>
            <div class="grid-card-actions">
              <button class="btn-icon move-up"   data-idx="${i}" title="Monter"    ${i === 0 ? 'disabled' : ''}>↑</button>
              <button class="btn-icon move-down" data-idx="${i}" title="Descendre" ${i === state.grids.length - 1 ? 'disabled' : ''}>↓</button>
              <button class="btn-sm btn-secondary reshuffle" data-idx="${i}" title="Re-mélanger">⟳</button>
            </div>
          </div>
          <div class="grid-preview">${renderMiniGrid(grid, i)}</div>
        </div>
      `;
    });
    html += `</div>`;
  }

  html += `</div>`;
  container.innerHTML = html;

  document.getElementById('btn-generate-all').addEventListener('click', () => {
    const btn = document.getElementById('btn-generate-all');
    btn.disabled = true;
    btn.textContent = '…';

    setTimeout(() => {
      const result = generateAll();
      const errEl  = document.getElementById('grids-error');
      if (result.error) {
        errEl.innerHTML = `<div class="error-msg">${esc(result.message)}</div>`;
        btn.disabled = false;
        btn.textContent = '⟳ Générer les grilles';
      } else {
        if (result.repeats) {
          errEl.innerHTML = `<div class="warn-banner">⚠ Moins de phrases que de cellules — certaines cases se répètent.</div>`;
        } else {
          errEl.innerHTML = '';
        }
        scheduleAutoSave();
        renderGrids();
      }
    }, 0);
  });

  container.querySelectorAll('.move-up').forEach(btn => {
    btn.addEventListener('click', () => {
      const i = parseInt(btn.dataset.idx);
      if (i > 0) {
        [state.grids[i - 1], state.grids[i]] = [state.grids[i], state.grids[i - 1]];
        scheduleAutoSave();
        renderGrids();
      }
    });
  });

  container.querySelectorAll('.move-down').forEach(btn => {
    btn.addEventListener('click', () => {
      const i = parseInt(btn.dataset.idx);
      if (i < state.grids.length - 1) {
        [state.grids[i], state.grids[i + 1]] = [state.grids[i + 1], state.grids[i]];
        scheduleAutoSave();
        renderGrids();
      }
    });
  });

  container.querySelectorAll('.reshuffle').forEach(btn => {
    btn.addEventListener('click', () => {
      reshuffleGrid(parseInt(btn.dataset.idx));
      scheduleAutoSave();
      renderGrids();
    });
  });

  attachGridInteractions(container);
}

// ── Drag & drop + click-to-replace ───────────────────────────────────────────

let _dragSrc = null;
let _interactionController = null;

function attachGridInteractions(container) {
  if (_interactionController) _interactionController.abort();
  _interactionController = new AbortController();
  const signal = _interactionController.signal;

  container.addEventListener('dragstart', e => {
    const td = e.target.closest('td[draggable="true"]');
    if (!td) return;
    _dragSrc = {
      grid: +td.dataset.grid,
      row:  +td.dataset.row,
      col:  +td.dataset.col,
    };
    td.classList.add('cell-dragging');
    e.dataTransfer.effectAllowed = 'move';
  }, { signal });

  container.addEventListener('dragend', () => {
    container.querySelectorAll('.cell-dragging, .cell-drag-over').forEach(el => {
      el.classList.remove('cell-dragging', 'cell-drag-over');
    });
    _dragSrc = null;
  }, { signal });

  container.addEventListener('dragover', e => {
    const td = e.target.closest('td[draggable="true"]');
    if (!td || !_dragSrc) return;
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
    container.querySelectorAll('.cell-drag-over').forEach(el => el.classList.remove('cell-drag-over'));
    td.classList.add('cell-drag-over');
  }, { signal });

  container.addEventListener('dragleave', e => {
    const td = e.target.closest('td[draggable="true"]');
    if (td) td.classList.remove('cell-drag-over');
  }, { signal });

  container.addEventListener('drop', e => {
    e.preventDefault();
    const td = e.target.closest('td[draggable="true"]');
    if (!td || !_dragSrc) return;
    const dst = { grid: +td.dataset.grid, row: +td.dataset.row, col: +td.dataset.col };
    if (dst.grid === _dragSrc.grid &&
        (dst.row !== _dragSrc.row || dst.col !== _dragSrc.col)) {
      const cells = state.grids[dst.grid].cells;
      const tmp = cells[_dragSrc.row][_dragSrc.col];
      cells[_dragSrc.row][_dragSrc.col] = cells[dst.row][dst.col];
      cells[dst.row][dst.col] = tmp;
      scheduleAutoSave();
      renderGrids();
    }
    _dragSrc = null;
  }, { signal });

  container.addEventListener('click', e => {
    if (_dragSrc) return;
    const td = e.target.closest('td[data-grid]');
    if (!td || td.classList.contains('free-cell')) return;
    showCellPicker(+td.dataset.grid, +td.dataset.row, +td.dataset.col);
  }, { signal });
}

function showCellPicker(gridIdx, row, col) {
  const currentCell = state.grids[gridIdx].cells[row][col];
  const cases = state.cases;

  const overlay = document.createElement('div');
  overlay.className = 'cell-picker-overlay';

  overlay.innerHTML = `
    <div class="cell-picker-modal" role="dialog" aria-modal="true">
      <div class="cell-picker-header">
        <span class="cell-picker-title">Remplacer la case</span>
        <button class="cell-picker-close" aria-label="Fermer">✕</button>
      </div>
      <div class="cell-picker-current">
        <span class="cpc-label">Actuelle :</span>
        <span class="cpc-value">${esc(currentCell.label)}</span>
      </div>
      <div class="cell-picker-search">
        <input class="cell-picker-filter" type="text" placeholder="Filtrer les phrases…" autocomplete="off">
      </div>
      <ul class="cell-picker-list">
        ${cases.map((c, i) => `
          <li class="cell-picker-item${c.label === currentCell.label ? ' current' : ''}" data-idx="${i}">
            <span class="cpi-label">${esc(c.label)}</span>
            <span class="cpi-pts">${c.points > 0 ? c.points + ' pts' : ''}</span>
          </li>
        `).join('')}
      </ul>
    </div>
  `;

  document.body.appendChild(overlay);

  const filterInput = overlay.querySelector('.cell-picker-filter');
  filterInput.focus();

  overlay.querySelector('.cell-picker-close').addEventListener('click', () => overlay.remove());
  overlay.addEventListener('click', e => { if (e.target === overlay) overlay.remove(); });
  document.addEventListener('keydown', function onKey(e) {
    if (e.key === 'Escape') { overlay.remove(); document.removeEventListener('keydown', onKey); }
  });

  filterInput.addEventListener('input', () => {
    const q = filterInput.value.toLowerCase();
    overlay.querySelectorAll('.cell-picker-item').forEach(li => {
      const matches = li.querySelector('.cpi-label').textContent.toLowerCase().includes(q);
      li.style.display = matches ? '' : 'none';
    });
  });

  overlay.querySelectorAll('.cell-picker-item').forEach(li => {
    li.addEventListener('click', () => {
      const chosen = cases[+li.dataset.idx];
      state.grids[gridIdx].cells[row][col] = { ...chosen };
      scheduleAutoSave();
      overlay.remove();
      renderGrids();
    });
  });
}

function renderMiniGrid(grid, gridIdx) {
  let html = `<table class="bingo-grid">`;
  grid.cells.forEach((rowCells, r) => {
    html += `<tr>`;
    rowCells.forEach((cell, c) => {
      const isFree = cell.isFree;
      html += `<td
        class="${isFree ? 'free-cell' : 'editable-cell'}"
        data-grid="${gridIdx}" data-row="${r}" data-col="${c}"
        ${!isFree ? 'draggable="true" title="Glisser pour déplacer · Cliquer pour remplacer"' : ''}
      >
        <span class="cell-label">${esc(cell.label)}</span>
        ${!isFree ? `<span class="cell-pts">${cell.points}</span>` : ''}
      </td>`;
    });
    html += `</tr>`;
  });
  html += `</table>`;
  return html;
}
