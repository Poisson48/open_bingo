import { state, scheduleAutoSave, gridsDirty } from './state.js';
import { generateAll, reshuffleGrid } from './generator.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

export function renderGrids() {
  const container = document.getElementById('grids-panel');

  let html = `
    <div class="grids-section">
      <div class="grids-header">
        <h2>Grilles générées <span class="badge">${state.grids.length}</span></h2>
        <button id="btn-generate-all" class="btn-primary">⟳ Générer toutes les grilles</button>
      </div>
      <div id="grids-error"></div>
      ${gridsDirty && state.grids.length > 0 ? `<div class="warn-banner">⚠ Les phrases ont été modifiées depuis la dernière génération — les grilles affichées peuvent être obsolètes.</div>` : ''}
  `;

  if (state.grids.length === 0) {
    html += `<p class="empty-state">Aucune grille générée. Configurez les joueurs et les cases, puis cliquez sur "Générer".</p>`;
  } else {
    html += `<div class="grids-list">`;
    state.grids.forEach((grid, i) => {
      html += `
        <div class="grid-card">
          <div class="grid-card-header">
            <span class="grid-player-name">${esc(grid.player)}</span>
            <div class="grid-card-actions">
              <button class="btn-icon move-up" data-idx="${i}" title="Monter" ${i === 0 ? 'disabled' : ''}>↑</button>
              <button class="btn-icon move-down" data-idx="${i}" title="Descendre" ${i === state.grids.length - 1 ? 'disabled' : ''}>↓</button>
              <button class="btn-sm btn-secondary reshuffle" data-idx="${i}">Re-mélanger</button>
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
    const result = generateAll();
    const errEl = document.getElementById('grids-error');
    if (result.error) {
      errEl.innerHTML = `<div class="error-msg">${esc(result.message)}</div>`;
    } else {
      errEl.innerHTML = result.repeats
        ? `<div class="warn-banner">⚠ Moins de phrases que de cellules — certaines cases apparaissent en double dans les grilles.</div>`
        : '';
      scheduleAutoSave();
      renderGrids();
    }
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
