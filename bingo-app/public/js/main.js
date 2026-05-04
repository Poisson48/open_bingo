import { state, exportJSON, importJSON } from './state.js';
import { renderConfig } from './config.js';
import { renderCases } from './cases.js';
import { renderGrids } from './grids.js';
import { renderPrint } from './print.js';
import { showToast } from './ui.js';

const tabPanels = {
  config: document.getElementById('tab-config'),
  cases: document.getElementById('tab-cases'),
  grids: document.getElementById('tab-grids'),
  print: document.getElementById('tab-print')
};

const tabBtns = document.querySelectorAll('.tab-btn');
let currentTab = 'config';

const renderers = {
  config: renderConfig,
  cases: renderCases,
  grids: renderGrids,
  print: renderPrint
};

function switchTab(name) {
  if (!tabPanels[name]) return;
  Object.entries(tabPanels).forEach(([k, el]) => {
    el.classList.toggle('active', k === name);
  });
  tabBtns.forEach(btn => {
    btn.classList.toggle('active', btn.dataset.tab === name);
  });
  currentTab = name;
  renderers[name]();
}

tabBtns.forEach(btn => {
  btn.addEventListener('click', () => switchTab(btn.dataset.tab));
});

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

renderConfig();
