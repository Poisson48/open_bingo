export const state = {
  title: 'Mon Bingo',
  gridSize: 5,
  players: [
    { name: 'Joueur 1' },
    { name: 'Joueur 2' }
  ],
  startHP: 20,
  freeCenter: true,
  multipliers: {
    line: 2,
    column: 2,
    diagonal: 3,
    full: 10
  },
  cases: [],
  gages: [],
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
    if (!Array.isArray(data.cases))   data.cases   = [];
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
