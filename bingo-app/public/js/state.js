export const state = {
  title: 'Bingo Complotiste — Da Vinci Code',
  gridSize: 5,
  players: [
    { name: 'Alice' },
    { name: 'Bob' },
    { name: 'Charlie' }
  ],
  startHP: 20,
  freeCenter: true,
  multipliers: {
    line: 2,
    column: 2,
    diagonal: 3,
    full: 10
  },
  cases: [
    { label: 'Illuminati mentionné',       points: 1, rate: 80 },
    { label: 'Reptiliens au pouvoir',       points: 2, rate: 60 },
    { label: 'La Terre est plate',          points: 1, rate: 70 },
    { label: '5G et micropuces',            points: 2, rate: 60 },
    { label: 'Vaccins et nanopuces',        points: 2, rate: 50 },
    { label: 'Deep State évoqué',           points: 1, rate: 70 },
    { label: 'Franc-maçonnerie citée',      points: 1, rate: 65 },
    { label: 'Lune artificielle',           points: 3, rate: 30 },
    { label: 'Chemtrails',                  points: 1, rate: 75 },
    { label: 'Complot Big Pharma',          points: 2, rate: 60 },
    { label: 'Da Vinci Code cité',          points: 1, rate: 90 },
    { label: 'Opus Dei mentionné',          points: 2, rate: 50 },
    { label: 'Fibonacci dans tout',         points: 2, rate: 55 },
    { label: 'Rose-Croix évoquée',          points: 1, rate: 50 },
    { label: 'Saint-Graal cherché',         points: 3, rate: 40 },
    { label: 'Chevaliers Templiers',        points: 1, rate: 65 },
    { label: 'Pyramide cachée',             points: 2, rate: 55 },
    { label: 'Code da Vinci trouvé',        points: 3, rate: 35 },
    { label: 'Jésus marié',                 points: 2, rate: 45 },
    { label: 'Mérovingiens cités',          points: 2, rate: 40 },
    { label: 'Vatican complice',            points: 1, rate: 70 },
    { label: 'Prieuré de Sion',             points: 2, rate: 50 },
    { label: 'Anagramme révélatrice',       points: 3, rate: 30 },
    { label: 'Tableau Renaissance caché',   points: 2, rate: 45 },
    { label: 'Complot mondial révélé',      points: 5, rate: 20 },
    { label: 'NWO mentionné',               points: 2, rate: 55 },
    { label: 'Shadow Government',           points: 1, rate: 60 },
    { label: 'Alien ancient theory',        points: 3, rate: 35 },
    { label: 'Simulation de Baudrillard',   points: 2, rate: 25 },
    { label: 'Matrice rouge/bleue',         points: 2, rate: 40 }
  ],
  gages: [
    { description: 'Faire un bonnet en papier alu et le porter 1 min',    hp: 2 },
    { description: 'Imiter un reptile pendant 10 secondes',               hp: 2 },
    { description: 'Chanter un couplet d\'une chanson complotiste',       hp: 3 },
    { description: 'Faire 10 pompes ou squats',                           hp: 3 },
    { description: 'Expliquer une théorie complotiste pendant 30 sec',    hp: 4 },
    { description: 'Mimer Indiana Jones à la recherche du Graal',         hp: 3 },
    { description: 'Dessiner la Joconde en moins de 30 secondes',         hp: 4 },
    { description: 'Trouver une anagramme d\'un prénom de la table',      hp: 5 },
    { description: 'Faire la bénédiction papale avec les bons gestes',    hp: 2 },
    { description: 'Rester en position de méditation 1 minute entière',  hp: 2 }
  ],
  grids: []
};

const STORAGE_KEY = 'bingo_state';
let _saveTimer = null;

export function saveState() {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
}

export function loadState() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return false;
    const data = JSON.parse(raw);
    if (!data || typeof data !== 'object') return false;
    if (!Array.isArray(data.cases))   data.cases   = state.cases;
    if (!Array.isArray(data.players)) data.players = state.players;
    if (!Array.isArray(data.gages))   data.gages   = [];
    if (!Array.isArray(data.grids))   data.grids   = [];
    if (!data.multipliers || typeof data.multipliers !== 'object') data.multipliers = state.multipliers;
    if (typeof data.gridSize !== 'number') data.gridSize = state.gridSize;
    if (typeof data.startHP  !== 'number') data.startHP  = state.startHP;
    Object.assign(state, data);
    return true;
  } catch {
    return false;
  }
}

export let gridsDirty = false;
export function markGridsDirty() { if (state.grids.length > 0) gridsDirty = true; }
export function clearGridsDirty() { gridsDirty = false; }

export function scheduleAutoSave() {
  clearTimeout(_saveTimer);
  _saveTimer = setTimeout(saveState, 400);
}

export function exportJSON() {
  const blob = new Blob([JSON.stringify(state, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `bingo-${state.title.replace(/[^a-z0-9]/gi, '_').toLowerCase()}-${Date.now()}.json`;
  a.click();
  URL.revokeObjectURL(url);
}

export function importJSON(json) {
  try {
    const data = JSON.parse(json);
    Object.assign(state, data);
    return true;
  } catch {
    return false;
  }
}
