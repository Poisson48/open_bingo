import { state, scheduleAutoSave } from './state.js';
import { showToast } from './ui.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

export function renderConfig() {
  const container = document.getElementById('config-form');

  // Badge onglet
  document.querySelector('[data-tab="config"]')?.setAttribute('data-count', state.players.length || '');

  const isEvenGrid = state.gridSize % 2 === 0;
  container.innerHTML = `
    <div class="config-section">

      <h2>Paramètres de la soirée</h2>
      <div class="section-card">
        <div class="form-group">
          <label for="cfg-title">Titre</label>
          <input type="text" id="cfg-title" value="${esc(state.title)}" placeholder="Ex: Bingo Complotiste — Da Vinci Code">
        </div>
        <div class="form-row">
          <div class="form-group">
            <label for="cfg-grid-size">Taille de grille (N×N)</label>
            <input type="number" id="cfg-grid-size" value="${state.gridSize}" min="2" max="12">
          </div>
          <div class="form-group" id="hp-group" ${state.gageMode ? 'style="display:none"' : ''}>
            <label for="cfg-hp">Points de vie de départ</label>
            <input type="number" id="cfg-hp" value="${state.startHP}" min="1" max="100">
          </div>
        </div>
        <div class="form-group">
          <label class="label-checkbox">
            <input type="checkbox" id="cfg-free-center" ${state.freeCenter ? 'checked' : ''} ${isEvenGrid ? 'disabled' : ''}>
            Case centrale FREE <span class="hint" style="margin:0;display:inline">(grilles impaires uniquement)</span>
          </label>
        </div>
        <div class="form-group" style="margin-top:8px;padding-top:12px;border-top:1px solid var(--border)">
          <label class="label-checkbox">
            <input type="checkbox" id="cfg-gage-mode" ${state.gageMode ? 'checked' : ''}>
            Mode Gage
          </label>
          <p class="hint" style="margin:4px 0 0 22px">
            Désactive les points de vie — le numéro affiché sur chaque case correspond au numéro du gage à effectuer.
          </p>
        </div>
      </div>

      <h2 id="multipliers-title" ${state.gageMode ? 'style="display:none"' : ''}>Multiplicateurs de points</h2>
      <div class="section-card" id="multipliers-card" ${state.gageMode ? 'style="display:none"' : ''}>
        <div class="form-row">
          <div class="form-group">
            <label for="cfg-mult-line">Ligne complète</label>
            <input type="number" id="cfg-mult-line" value="${state.multipliers.line}" min="1">
          </div>
          <div class="form-group">
            <label for="cfg-mult-col">Colonne complète</label>
            <input type="number" id="cfg-mult-col" value="${state.multipliers.column}" min="1">
          </div>
          <div class="form-group">
            <label for="cfg-mult-diag">Diagonale complète</label>
            <input type="number" id="cfg-mult-diag" value="${state.multipliers.diagonal}" min="1">
          </div>
          <div class="form-group">
            <label for="cfg-mult-full">Grille complète (BINGO)</label>
            <input type="number" id="cfg-mult-full" value="${state.multipliers.full}" min="1">
          </div>
        </div>
      </div>

      <h2>Joueurs <span class="badge">${state.players.length}</span></h2>
      <div class="section-card">
        <div id="players-list">
          ${state.players.map((p, i) => renderPlayerRow(p, i)).join('')}
        </div>
        <button id="add-player" class="btn-secondary">+ Ajouter un joueur</button>
      </div>

      <div class="form-actions">
        <button id="save-config" class="btn-primary">Enregistrer la configuration</button>
        <button id="reset-state" class="btn-danger" style="margin-left:auto">Réinitialiser…</button>
      </div>
    </div>
  `;

  document.querySelectorAll('.player-name').forEach(input => {
    input.addEventListener('input', () => {
      const idx = parseInt(input.dataset.idx);
      if (state.players[idx] !== undefined) {
        state.players[idx].name = input.value;
        scheduleAutoSave();
      }
    });
  });

  document.getElementById('cfg-grid-size').addEventListener('input', (e) => {
    const n  = parseInt(e.target.value) || 5;
    const cb = document.getElementById('cfg-free-center');
    cb.disabled = (n % 2 === 0);
    if (n % 2 === 0) cb.checked = false;
  });

  document.getElementById('cfg-gage-mode').addEventListener('change', (e) => {
    const on = e.target.checked;
    document.getElementById('hp-group').style.display          = on ? 'none' : '';
    document.getElementById('multipliers-title').style.display = on ? 'none' : '';
    document.getElementById('multipliers-card').style.display  = on ? 'none' : '';
  });

  document.getElementById('add-player').addEventListener('click', () => {
    state.players.push({ name: '' });
    scheduleAutoSave();
    renderConfig();
    const inputs = document.querySelectorAll('.player-name');
    inputs[inputs.length - 1]?.focus();
  });

  document.querySelectorAll('.remove-player').forEach(btn => {
    btn.addEventListener('click', () => {
      state.players.splice(parseInt(btn.dataset.idx), 1);
      scheduleAutoSave();
      renderConfig();
    });
  });

  document.getElementById('save-config').addEventListener('click', saveConfig);

  document.getElementById('reset-state').addEventListener('click', () => {
    if (confirm('Réinitialiser ce projet ? Toutes les phrases, gages et grilles seront effacés.')) {
      state.gridSize    = 5;
      state.startHP     = 20;
      state.freeCenter  = true;
      state.gageMode    = false;
      state.multipliers = { line: 2, column: 2, diagonal: 3, full: 10 };
      state.players     = [{ name: 'Joueur 1' }, { name: 'Joueur 2' }];
      state.cases       = [];
      state.gages       = [];
      state.grids       = [];
      scheduleAutoSave();
      renderConfig();
      showToast('Projet réinitialisé.');
    }
  });
}

function renderPlayerRow(p, i) {
  return `
    <div class="player-row">
      <span class="player-num">${i + 1}</span>
      <input type="text" class="player-name" data-idx="${i}" value="${esc(p.name)}" placeholder="Prénom du joueur">
      <button class="btn-icon remove-player" data-idx="${i}" title="Supprimer">✕</button>
    </div>
  `;
}

function saveConfig() {
  state.title    = document.getElementById('cfg-title').value.trim() || 'Bingo';
  state.gridSize = Math.max(2, Math.min(12, parseInt(document.getElementById('cfg-grid-size').value) || 5));
  state.gageMode = document.getElementById('cfg-gage-mode').checked;
  state.startHP  = state.gageMode ? state.startHP : Math.max(1, Math.min(100, parseInt(document.getElementById('cfg-hp').value) || 20));
  state.freeCenter = document.getElementById('cfg-free-center').checked;
  state.multipliers.line     = Math.max(1, parseInt(document.getElementById('cfg-mult-line').value)  || 1);
  state.multipliers.column   = Math.max(1, parseInt(document.getElementById('cfg-mult-col').value)   || 1);
  state.multipliers.diagonal = Math.max(1, parseInt(document.getElementById('cfg-mult-diag').value)  || 1);
  state.multipliers.full     = Math.max(1, parseInt(document.getElementById('cfg-mult-full').value)  || 1);

  document.querySelectorAll('.player-name').forEach((input) => {
    const idx = parseInt(input.dataset.idx);
    if (state.players[idx] !== undefined) state.players[idx].name = input.value.trim();
  });
  state.players = state.players.filter(p => p.name !== '');
  state.grids   = [];

  document.getElementById('app-title').textContent = state.title;
  scheduleAutoSave();
  showToast('Configuration enregistrée — les grilles ont été réinitialisées.');
  renderConfig();
}
