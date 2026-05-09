import { getAllProjects, createProject, cloneProject, deleteProject, updateProjectMeta, exportProjectById } from './state.js';
import { showToast } from './ui.js';

const esc = (s) => String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

const ACCENT_PALETTE = [
  '#6366f1','#8b5cf6','#a855f7','#ec4899',
  '#f43f5e','#f59e0b','#10b981','#3b82f6',
  '#06b6d4','#84cc16'
];

function cardAccent(id) {
  let h = 0;
  for (let i = 0; i < id.length; i++) h = (h * 31 + id.charCodeAt(i)) | 0;
  return ACCENT_PALETTE[Math.abs(h) % ACCENT_PALETTE.length];
}

function fmtDate(ts) {
  if (!ts) return '—';
  const d = new Date(ts);
  const now = new Date();
  const diff = now - d;
  if (diff < 60000)    return 'à l\'instant';
  if (diff < 3600000)  return `il y a ${Math.floor(diff / 60000)} min`;
  if (diff < 86400000) return `il y a ${Math.floor(diff / 3600000)} h`;
  return d.toLocaleDateString('fr-FR', { day: '2-digit', month: '2-digit', year: 'numeric' });
}

let _onOpen = null;
let _searchVal = '';

export function renderProjects(onOpen) {
  _onOpen = onOpen;
  const panel = document.getElementById('projects-panel');
  panel.innerHTML = `
<div class="projects-toolbar">
  <button class="btn-primary" id="btn-new-project">+ Nouveau projet</button>
  <div class="projects-search">
    <input type="text" id="projects-search" placeholder="Rechercher…" value="${esc(_searchVal)}" autocomplete="off">
  </div>
</div>
<div id="projects-grid-container"></div>
`;
  _renderGrid();
  _bindToolbar();
}

function _renderGrid() {
  const container = document.getElementById('projects-grid-container');
  if (!container) return;

  const q = _searchVal.trim().toLowerCase();
  const all = getAllProjects();
  const filtered = q
    ? all.filter(p => p.title.toLowerCase().includes(q) || (p.description || '').toLowerCase().includes(q))
    : all;
  const sorted = [...filtered].sort((a, b) => (b.updatedAt || 0) - (a.updatedAt || 0));

  if (sorted.length === 0) {
    container.innerHTML = `<div class="empty-state">
      <span class="empty-icon">🎱</span>
      <strong>${q ? 'Aucun résultat' : 'Aucun projet'}</strong>
      ${q ? 'Essayez un autre mot-clé.' : 'Créez votre premier bingo avec le bouton ci-dessus !'}
    </div>`;
  } else {
    container.innerHTML = `<div class="projects-grid">${sorted.map(_cardHTML).join('')}</div>`;
  }
  _bindCardEvents();
}

function _cardHTML(p) {
  const ncases   = Array.isArray(p.cases)   ? p.cases.length   : 0;
  const nplayers = Array.isArray(p.players) ? p.players.length : 0;
  const ngrids   = Array.isArray(p.grids)   ? p.grids.length   : 0;
  const size     = p.gridSize || '?';
  const accent   = cardAccent(p.id);

  return `
<div class="project-card" data-id="${esc(p.id)}" style="--card-accent: ${accent}">
  <div class="project-card-header">
    <div class="project-card-title">${esc(p.title)}</div>
    <div class="project-card-actions">
      <button class="btn-icon proj-edit"   data-id="${esc(p.id)}" title="Modifier">✏</button>
      <button class="btn-icon proj-clone"  data-id="${esc(p.id)}" title="Cloner">⧉</button>
      <button class="btn-icon proj-export" data-id="${esc(p.id)}" title="Exporter">↓</button>
      <button class="btn-icon proj-delete remove-case" data-id="${esc(p.id)}" title="Supprimer">✕</button>
    </div>
  </div>
  ${p.description ? `<div class="project-card-desc">${esc(p.description)}</div>` : ''}
  <div class="project-card-meta">
    <span class="meta-chip">${size}×${size}</span>
    <span class="meta-chip">${nplayers} joueur${nplayers > 1 ? 's' : ''}</span>
    <span class="meta-chip">${ncases} case${ncases > 1 ? 's' : ''}</span>
    ${ngrids > 0 ? `<span class="meta-chip">${ngrids} grille${ngrids > 1 ? 's' : ''}</span>` : ''}
    <span class="sep" style="margin-left:auto">${fmtDate(p.updatedAt)}</span>
  </div>
  <button class="btn-primary proj-open" data-id="${esc(p.id)}">Ouvrir →</button>
</div>`;
}

function _bindToolbar() {
  document.getElementById('btn-new-project').addEventListener('click', () => {
    _onOpen(createProject());
  });

  document.getElementById('projects-search').addEventListener('input', (e) => {
    _searchVal = e.target.value;
    _renderGrid();
  });
}

function _bindCardEvents() {
  const container = document.getElementById('projects-grid-container');
  if (!container) return;

  container.querySelectorAll('.proj-open').forEach(btn => {
    btn.addEventListener('click', () => _onOpen(btn.dataset.id));
  });

  container.querySelectorAll('.proj-edit').forEach(btn => {
    btn.addEventListener('click', () => _openEditModal(btn.dataset.id));
  });

  container.querySelectorAll('.proj-clone').forEach(btn => {
    btn.addEventListener('click', () => {
      const newId = cloneProject(btn.dataset.id);
      if (newId) { _renderGrid(); showToast('Projet cloné !'); }
    });
  });

  container.querySelectorAll('.proj-export').forEach(btn => {
    btn.addEventListener('click', () => exportProjectById(btn.dataset.id));
  });

  container.querySelectorAll('.proj-delete').forEach(btn => {
    btn.addEventListener('click', () => {
      const p = getAllProjects().find(x => x.id === btn.dataset.id);
      if (!p || !confirm(`Supprimer "${p.title}" ? Cette action est irréversible.`)) return;
      deleteProject(p.id);
      _renderGrid();
      showToast('Projet supprimé.');
    });
  });
}

function _openEditModal(id) {
  const p = getAllProjects().find(x => x.id === id);
  if (!p) return;

  document.getElementById('project-edit-modal')?.remove();

  const modal = document.createElement('div');
  modal.id        = 'project-edit-modal';
  modal.className = 'modal-overlay';
  modal.innerHTML = `
<div class="modal-box">
  <h3>Modifier le projet</h3>
  <div class="form-group">
    <label>Titre</label>
    <input type="text" id="edit-proj-title" value="${esc(p.title)}" maxlength="80">
  </div>
  <div class="form-group">
    <label>Description <span style="font-weight:400;text-transform:none">(optionnelle)</span></label>
    <input type="text" id="edit-proj-desc" value="${esc(p.description || '')}" maxlength="200" placeholder="Soirée jeu du 12 juin…">
  </div>
  <div class="modal-actions">
    <button class="btn-primary"   id="edit-proj-save">Enregistrer</button>
    <button class="btn-secondary" id="edit-proj-cancel">Annuler</button>
  </div>
</div>`;
  document.body.appendChild(modal);

  const titleInput = modal.querySelector('#edit-proj-title');
  titleInput.focus();
  titleInput.select();

  const save = () => {
    updateProjectMeta(id, {
      title:       modal.querySelector('#edit-proj-title').value,
      description: modal.querySelector('#edit-proj-desc').value
    });
    modal.remove();
    _renderGrid();
    showToast('Projet mis à jour !');
  };
  const close = () => modal.remove();

  modal.querySelector('#edit-proj-save').addEventListener('click', save);
  modal.querySelector('#edit-proj-cancel').addEventListener('click', close);
  modal.addEventListener('click', (e) => { if (e.target === modal) close(); });
  modal.addEventListener('keydown', (e) => {
    if (e.key === 'Enter')  save();
    if (e.key === 'Escape') close();
  });
}
