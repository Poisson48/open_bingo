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
  if (N <= 3)      { fontSize = '18px'; cellPad = '8px 5px'; }
  else if (N <= 5) { fontSize = '13px'; cellPad = '5px 3px'; }
  else if (N <= 7) { fontSize = '10px'; cellPad = '3px 2px'; }
  else             { fontSize = '7px';  cellPad = '2px 1px'; }

  let gridRows = '';
  for (const row of grid.cells) {
    gridRows += `<tr>`;
    for (const cell of row) {
      gridRows += `<td class="${cell.isFree ? 'free-cell' : ''}" style="padding:${cellPad}">
        <span class="cell-label">${esc(cell.label)}</span>
        ${!cell.isFree ? `<span class="cell-points${state.gageMode ? ' cell-gage-ref' : ''}">${state.gageMode ? '#' + cell.points : cell.points}</span>` : ''}
      </td>`;
    }
    gridRows += `</tr>`;
  }

  const hpBoxes = Array(state.startHP).fill('<span class="hp-box"></span>').join('');

  const bottomSection = state.gageMode ? `
    <div class="rules-section">
      <div class="rules-note">
        Le numéro dans le coin bas-droit de chaque case correspond au <strong>numéro du gage</strong> à effectuer (voir tableau des gages).
      </div>
    </div>
  ` : `
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
  `;

  return `
    <div class="print-sheet">
      <div class="sheet-header">
        <div class="sheet-title">${esc(state.title)}</div>
        <div class="sheet-player">${esc(grid.player)}</div>
      </div>

      <div class="print-grid-wrapper">
        <table class="print-grid" style="font-size:${fontSize}">
          ${gridRows}
        </table>
      </div>

      ${bottomSection}
    </div>
  `;
}

function renderGageSheet() {
  const rows = state.gages.map((g, i) => `
    <tr>
      <td class="gage-num">${i + 1}</td>
      <td>${esc(g.description)}</td>
      ${!state.gageMode ? `<td class="gage-hp-val">+${g.hp} PV</td>` : ''}
    </tr>
  `).join('');

  const intro = state.gageMode
    ? 'Le numéro sur chaque case de la grille renvoie au gage correspondant. Effectuez-le quand vous tombez dessus !'
    : 'Accomplissez n\'importe quel gage pour récupérer des points de vie. Une fois accompli, cochez-le.';

  const combos = state.comboGages || {};
  const hasCombo = state.gageMode && (combos.line || combos.column || combos.diagonal);
  const comboSection = hasCombo ? `
    <div class="combo-gages-print">
      <div class="combo-gages-title">Gages de Combinaison</div>
      <table class="combo-gages-table">
        <tbody>
          ${combos.line     ? `<tr><td class="combo-type">Ligne complète</td><td>${esc(combos.line)}</td></tr>` : ''}
          ${combos.column   ? `<tr><td class="combo-type">Colonne complète</td><td>${esc(combos.column)}</td></tr>` : ''}
          ${combos.diagonal ? `<tr><td class="combo-type">Diagonale complète</td><td>${esc(combos.diagonal)}</td></tr>` : ''}
        </tbody>
      </table>
    </div>
  ` : '';

  return `
    <div class="print-sheet gage-sheet">
      <div class="sheet-header">
        <div class="sheet-title">${esc(state.title)}</div>
        <div class="sheet-player">Tableau des Gages</div>
      </div>
      <p class="gage-intro">${intro}</p>
      <table class="gage-table">
        <thead>
          <tr>
            <th style="width:32px">#</th>
            <th>Gage</th>
            ${!state.gageMode ? '<th style="width:70px">PV récupérés</th>' : ''}
          </tr>
        </thead>
        <tbody>${rows}</tbody>
      </table>
      ${comboSection}
    </div>
  `;
}
