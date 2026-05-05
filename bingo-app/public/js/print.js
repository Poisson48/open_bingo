import { state } from './state.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

export function renderPrint() {
  const container = document.getElementById('print-panel');

  if (state.grids.length === 0) {
    container.innerHTML = `
      <div class="print-empty">
        <span class="empty-icon">🖨</span>
        <p>Aucune grille à imprimer.</p>
        <p>Allez dans l'onglet <strong>Grilles</strong> pour en générer d'abord.</p>
      </div>
    `;
    return;
  }

  const hasGages = state.gages?.length > 0;
  const playerSheets = state.grids.map(grid => renderSheet(grid, hasGages));
  const playerPages = [];
  for (let i = 0; i < playerSheets.length; i += 2) {
    playerPages.push(`<div class="print-page">${playerSheets.slice(i, i + 2).join('')}</div>`);
  }
  const gagePage = hasGages ? `<div class="print-page gage-page">${renderGageSheet()}</div>` : '';
  const pagesCount = playerPages.length + (hasGages ? 1 : 0);

  container.innerHTML = `
    <div class="print-controls no-print">
      <button id="btn-print" class="btn-primary">🖨 Imprimer (${pagesCount} page${pagesCount > 1 ? 's' : ''} A4)</button>
      <span class="print-info">${state.grids.length} grille${state.grids.length > 1 ? 's' : ''} joueur${state.grids.length > 1 ? 's' : ''}${hasGages ? ' + 1 page gages' : ''} — 2 grilles par feuille</span>
    </div>
    <div id="print-content">
      ${playerPages.join('')}${gagePage}
    </div>
  `;

  document.getElementById('btn-print').addEventListener('click', () => {
    if (window.AndroidPrint) {
      window.AndroidPrint.print();
    } else {
      window.print();
    }
  });

  requestAnimationFrame(() => scalePreviewToFit());
}

function scalePreviewToFit() {
  const content = document.getElementById('print-content');
  if (!content) return;
  const page = content.querySelector('.print-page');
  if (!page) return;
  const available = document.documentElement.clientWidth;
  const pageW = page.scrollWidth;
  if (pageW > available) {
    const scale = available / pageW;
    content.querySelectorAll('.print-page').forEach(p => { p.style.zoom = scale; });
  }
}

function renderSheet(grid, hasGages) {
  const N = state.gridSize;

  let fontSize, cellPad;
  if (N <= 3)      { fontSize = '16px'; cellPad = '8px 5px'; }
  else if (N <= 5) { fontSize = '12px'; cellPad = '5px 3px'; }
  else if (N <= 7) { fontSize = '9px';  cellPad = '3px 2px'; }
  else             { fontSize = '7px';  cellPad = '2px 1px'; }

  const cellHeightMm = Math.max(6, Math.floor(70 / N));

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
