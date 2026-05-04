import { state } from './state.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

export function renderPrint() {
  const container = document.getElementById('print-panel');

  if (state.grids.length === 0) {
    container.innerHTML = `
      <div class="print-empty">
        <p>Aucune grille à afficher.</p>
        <p>Allez dans l'onglet <strong>Grilles</strong> pour générer les grilles d'abord.</p>
      </div>
    `;
    return;
  }

  const hasGages = state.gages?.length > 0;
  const sheetsCount = state.grids.length + (hasGages ? 1 : 0);

  container.innerHTML = `
    <div class="print-controls no-print">
      <button id="btn-print" class="btn-primary">🖨 Imprimer (${sheetsCount} feuille${sheetsCount > 1 ? 's' : ''})</button>
      <span class="print-info">${state.grids.length} grille${state.grids.length > 1 ? 's' : ''} joueur${state.grids.length > 1 ? 's' : ''}${hasGages ? ' + 1 tableau de gages' : ''}</span>
    </div>
    <div id="print-content">
      ${state.grids.map((grid, i) => renderSheet(grid, hasGages)).join('')}
      ${hasGages ? renderGageSheet() : ''}
    </div>
  `;

  document.getElementById('btn-print').addEventListener('click', () => window.print());
}

function renderSheet(grid, hasGages) {
  const N = state.gridSize;

  let fontSize, cellPad;
  if (N <= 3)      { fontSize = '18px'; cellPad = '10px 6px'; }
  else if (N <= 5) { fontSize = '13px'; cellPad = '6px 4px'; }
  else if (N <= 7) { fontSize = '10px'; cellPad = '4px 3px'; }
  else             { fontSize = '8px';  cellPad = '2px'; }

  const cellHeightMm = Math.max(8, Math.floor(155 / N));

  let gridRows = '';
  for (const row of grid.cells) {
    gridRows += `<tr style="height:${cellHeightMm}mm">`;
    for (const cell of row) {
      gridRows += `<td class="${cell.isFree ? 'free-cell' : ''}" style="padding:${cellPad}">
        <span class="cell-label">${esc(cell.label)}</span>
        ${!cell.isFree ? `<span class="cell-points">${cell.points}</span>` : ''}
      </td>`;
    }
    gridRows += `</tr>`;
  }

  const hpBoxes = Array(state.startHP).fill('<span class="hp-box"></span>').join('');

  return `
    <div class="print-sheet">
      <div class="sheet-header">
        <div class="sheet-title">${esc(state.title)}</div>
        <div class="sheet-player">${esc(grid.player)}</div>
      </div>

      <table class="print-grid" style="font-size:${fontSize}">
        ${gridRows}
      </table>

      <div class="hp-section">
        <div class="hp-label">Points de vie — ${state.startHP} PV</div>
        <div class="hp-gauge">${hpBoxes}</div>
      </div>

      <div class="rules-section">
        <div class="rules-title">Règles des combinaisons</div>
        <table class="rules-table">
          <tr>
            <td>Ligne complète</td>
            <td>valeur × <strong>${state.multipliers.line}</strong></td>
            <td>Colonne complète</td>
            <td>valeur × <strong>${state.multipliers.column}</strong></td>
          </tr>
          <tr>
            <td>Diagonale complète</td>
            <td>valeur × <strong>${state.multipliers.diagonal}</strong></td>
            <td>Grille complète (BINGO)</td>
            <td>valeur × <strong>${state.multipliers.full}</strong></td>
          </tr>
        </table>
        <div class="rules-note">
          Cochez une case quand l'événement se produit dans le film. La valeur en points est dans le coin bas-droit.
          ${hasGages ? 'Pour récupérer des PV, accomplissez un gage du <strong>tableau de gages</strong> (feuille séparée).' : ''}
        </div>
      </div>
    </div>
  `;
}

function renderGageSheet() {
  const rows = state.gages.map((g, i) => `
    <tr>
      <td class="gage-num">${i + 1}</td>
      <td>${esc(g.description)}</td>
      <td class="gage-hp-val">+${g.hp} PV</td>
    </tr>
  `).join('');

  return `
    <div class="print-sheet gage-sheet">
      <div class="sheet-header">
        <div class="sheet-title">${esc(state.title)}</div>
        <div class="sheet-player">Tableau des Gages</div>
      </div>
      <p class="gage-intro">Accomplissez n'importe quel gage pour récupérer des points de vie. Une fois accompli, cochez-le.</p>
      <table class="gage-table">
        <thead>
          <tr>
            <th style="width:32px">#</th>
            <th>Gage</th>
            <th style="width:70px">PV récupérés</th>
          </tr>
        </thead>
        <tbody>${rows}</tbody>
      </table>
    </div>
  `;
}
