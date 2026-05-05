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
          <div class="grid-preview">${renderMiniGrid(grid)}</div>
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
}

function renderMiniGrid(grid) {
  let html = `<table class="bingo-grid">`;
  for (const row of grid.cells) {
    html += `<tr>`;
    for (const cell of row) {
      html += `<td class="${cell.isFree ? 'free-cell' : ''}">
        <span class="cell-label">${esc(cell.label)}</span>
        ${!cell.isFree ? `<span class="cell-pts">${cell.points}</span>` : ''}
      </td>`;
    }
    html += `</tr>`;
  }
  html += `</table>`;
  return html;
}
