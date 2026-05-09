import {
  state,
  exportJSON, importJSON,
  exportAllProjects, importAllProjects,
  loadProject, loadLastProject,
  saveCurrentProject, scheduleAutoSave
} from './state.js';
import { renderConfig } from './config.js';
import { renderCases }  from './cases.js';
import { renderGrids }  from './grids.js';
import { renderPrint }  from './print.js';
import { renderPlay }  from './play.js';
import { renderProjects } from './projects.js';
import { showToast } from './ui.js';

// ── Views ──────────────────────────────────────────────────────────────────────
const viewProjects = document.getElementById('view-projects');
const viewEditor   = document.getElementById('view-editor');

function showProjectsView() {
  saveCurrentProject();
  viewEditor.classList.remove('active');
  viewProjects.classList.add('active');
  renderProjects(openProject);
}

function openProject(id) {
  if (!loadProject(id)) return;
  document.getElementById('app-title').textContent = state.title;
  viewProjects.classList.remove('active');
  viewEditor.classList.add('active');
  switchTab('config');
}

// ── Tabs ───────────────────────────────────────────────────────────────────────
const tabPanels = {
  config: document.getElementById('tab-config'),
  cases:  document.getElementById('tab-cases'),
  grids:  document.getElementById('tab-grids'),
  print:  document.getElementById('tab-print'),
  play:   document.getElementById('tab-play')
};
const tabBtns = document.querySelectorAll('.tab-btn');
let currentTab = 'config';

const renderers = {
  config: renderConfig,
  cases:  renderCases,
  grids:  renderGrids,
  print:  renderPrint,
  play:   renderPlay
};

function switchTab(name) {
  if (!tabPanels[name]) return;
  Object.entries(tabPanels).forEach(([k, el]) => el.classList.toggle('active', k === name));
  tabBtns.forEach(btn => btn.classList.toggle('active', btn.dataset.tab === name));
  currentTab = name;
  renderers[name]();
}

tabBtns.forEach(btn => btn.addEventListener('click', () => switchTab(btn.dataset.tab)));

// ── Navigation ─────────────────────────────────────────────────────────────────
document.getElementById('btn-back-projects').addEventListener('click', showProjectsView);

// ── Export / Import (projet courant) ──────────────────────────────────────────
document.getElementById('btn-export').addEventListener('click', exportJSON);

document.getElementById('import-file').addEventListener('change', (e) => {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = (ev) => {
    if (importJSON(ev.target.result)) {
      document.getElementById('app-title').textContent = state.title;
      renderers[currentTab]();
      showToast('Import réussi !');
    } else {
      showToast('Erreur : fichier JSON invalide.', true);
    }
    e.target.value = '';
  };
  reader.readAsText(file);
});

// ── Export / Import (tous les projets) ────────────────────────────────────────
document.getElementById('btn-export-all').addEventListener('click', exportAllProjects);

document.getElementById('import-all-file').addEventListener('change', (e) => {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = (ev) => {
    const count = importAllProjects(ev.target.result);
    if (count !== false && count > 0) {
      renderProjects(openProject);
      showToast(`${count} projet${count > 1 ? 's' : ''} importé${count > 1 ? 's' : ''} !`);
    } else {
      showToast('Erreur : fichier invalide.', true);
    }
    e.target.value = '';
  };
  reader.readAsText(file);
});

// ── Auto-save ──────────────────────────────────────────────────────────────────
document.addEventListener('input', scheduleAutoSave);

// ── Version ────────────────────────────────────────────────────────────────────
fetch('/version.json')
  .then(r => r.json())
  .then(({ version }) => {
    document.querySelectorAll('.app-version').forEach(el => { el.textContent = `v${version}`; });
  })
  .catch(() => {});

// ── Init ───────────────────────────────────────────────────────────────────────
loadLastProject();
viewProjects.classList.add('active');
renderProjects(openProject);
