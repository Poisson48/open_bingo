import { state } from './state.js';
import { showToast } from './ui.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

export function renderConfig() {
  const container = document.getElementById('config-form');
  container.innerHTML = `
    <div class="config-section">
      <h2>Paramètres globaux</h2>

      <div class="form-group">
        <label for="cfg-title">Titre de la soirée</label>
        <input type="text" id="cfg-title" value="${esc(state.title)}" placeholder="Ex: Bingo Complotiste — Da Vinci Code">
      </div>

      <div class="form-row">
        <div class="form-group">
          <label for="cfg-grid-size">Taille de grille (N×N)</label>
          <input type="number" id="cfg-grid-size" value="${state.gridSize}" min="2" max="12">
        </div>
        <div class="form-group">
          <label for="cfg-hp">Points de vie de départ</label>
          <input type="number" id="cfg-hp" value="${state.startHP}" min="1" max="100">
        </div>
      </div>

      <div class="form-group">
        <label class="label-checkbox">
          <input type="checkbox" id="cfg-free-center" ${state.freeCenter ? 'checked' : ''}>
          Case centrale FREE (uniquement si N est impair)
        </label>
        <span class="hint">Chaque case a son propre taux de réapparition, configurable dans l'onglet Cases.</span>
      </div>

      <h2>Multiplicateurs de points</h2>
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
          <label for="cfg-mult-full">Grille complète</label>
          <input type="number" id="cfg-mult-full" value="${state.multipliers.full}" min="1">
        </div>
      </div>

      <h2>Joueurs <span class="badge">${state.players.length}</span></h2>
      <div id="players-list">
        ${state.players.map((p, i) => renderPlayerRow(p, i)).join('')}
      </div>
      <button id="add-player" class="btn-secondary">+ Ajouter un joueur</button>

      <div class="form-actions">
        <button id="save-config" class="btn-primary">Enregistrer la configuration</button>
      </div>
    </div>
  `;

  document.getElementById('cfg-common-rate').addEventListener('input', (e) => {
    document.getElementById('common-rate-val').textContent = e.target.value + '%';
  });

  document.getElementById('add-player').addEventListener('click', () => {
    state.players.push({ name: '' });
    renderConfig();
    const inputs = document.querySelectorAll('.player-name');
    inputs[inputs.length - 1]?.focus();
  });

  document.querySelectorAll('.remove-player').forEach(btn => {
    btn.addEventListener('click', () => {
      state.players.splice(parseInt(btn.dataset.idx), 1);
      renderConfig();
    });
  });

  document.getElementById('save-config').addEventListener('click', saveConfig);
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
  state.title = document.getElementById('cfg-title').value.trim() || 'Bingo';
  state.gridSize = Math.max(2, Math.min(12, parseInt(document.getElementById('cfg-grid-size').value) || 5));
  state.startHP = Math.max(1, Math.min(100, parseInt(document.getElementById('cfg-hp').value) || 20));
  state.freeCenter = document.getElementById('cfg-free-center').checked;
  state.multipliers.line = Math.max(1, parseInt(document.getElementById('cfg-mult-line').value) || 1);
  state.multipliers.column = Math.max(1, parseInt(document.getElementById('cfg-mult-col').value) || 1);
  state.multipliers.diagonal = Math.max(1, parseInt(document.getElementById('cfg-mult-diag').value) || 1);
  state.multipliers.full = Math.max(1, parseInt(document.getElementById('cfg-mult-full').value) || 1);

  document.querySelectorAll('.player-name').forEach((input) => {
    const idx = parseInt(input.dataset.idx);
    if (state.players[idx] !== undefined) state.players[idx].name = input.value.trim();
  });
  state.players = state.players.filter(p => p.name !== '');

  document.getElementById('app-title').textContent = state.title;
  state.grids = [];
  showToast('Configuration enregistrée — les grilles ont été réinitialisées.');
  renderConfig();
}
