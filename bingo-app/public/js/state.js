const PROJECTS_KEY = 'bingo_projects';
const CURRENT_KEY  = 'bingo_current_project_id';

function makeId() {
  return 'proj_' + Date.now() + '_' + Math.random().toString(36).slice(2, 7);
}

function defaultProject(overrides = {}) {
  return {
    id: makeId(),
    title: 'Nouveau Bingo',
    description: '',
    createdAt: Date.now(),
    updatedAt: Date.now(),
    gridSize: 5,
    players: [{ name: 'Joueur 1' }, { name: 'Joueur 2' }],
    startHP: 20,
    freeCenter: true,
    gageMode: false,
    comboGages: { line: '', column: '', diagonal: '' },
    multipliers: { line: 2, column: 2, diagonal: 3, full: 10 },
    cases: [],
    gages: [],
    grids: [],
    ...overrides
  };
}

export const state = defaultProject();

// ── Projects persistence ──────────────────────────────────────────────────────

export function getAllProjects() {
  try {
    const raw = localStorage.getItem(PROJECTS_KEY);
    if (!raw) return [];
    const arr = JSON.parse(raw);
    return Array.isArray(arr) ? arr : [];
  } catch { return []; }
}

function _saveProjects(projects) {
  localStorage.setItem(PROJECTS_KEY, JSON.stringify(projects));
}

export function saveCurrentProject() {
  state.updatedAt = Date.now();
  const projects = getAllProjects();
  const idx = projects.findIndex(p => p.id === state.id);
  if (idx >= 0) projects[idx] = { ...state };
  else projects.push({ ...state });
  _saveProjects(projects);
  localStorage.setItem(CURRENT_KEY, state.id);
}

export function loadProject(id) {
  const p = getAllProjects().find(p => p.id === id);
  if (!p) return false;
  Object.assign(state, p);
  localStorage.setItem(CURRENT_KEY, id);
  return true;
}

export function createProject(overrides = {}) {
  const p = defaultProject(overrides);
  const projects = getAllProjects();
  projects.push(p);
  _saveProjects(projects);
  Object.assign(state, p);
  localStorage.setItem(CURRENT_KEY, p.id);
  return p.id;
}

export function cloneProject(id) {
  const projects = getAllProjects();
  const original = projects.find(p => p.id === id);
  if (!original) return null;
  const clone = {
    ...original,
    id: makeId(),
    title: original.title + ' (copie)',
    createdAt: Date.now(),
    updatedAt: Date.now(),
    grids: []
  };
  const idx = projects.findIndex(p => p.id === id);
  projects.splice(idx + 1, 0, clone);
  _saveProjects(projects);
  return clone.id;
}

export function deleteProject(id) {
  _saveProjects(getAllProjects().filter(p => p.id !== id));
  if (localStorage.getItem(CURRENT_KEY) === id) {
    localStorage.removeItem(CURRENT_KEY);
  }
}

export function updateProjectMeta(id, { title, description }) {
  const projects = getAllProjects();
  const p = projects.find(p => p.id === id);
  if (!p) return;
  if (title !== undefined) p.title = title.trim() || p.title;
  if (description !== undefined) p.description = description;
  p.updatedAt = Date.now();
  _saveProjects(projects);
  if (state.id === id) {
    if (title !== undefined) state.title = p.title;
    if (description !== undefined) state.description = p.description;
  }
}

export function loadLastProject() {
  let projects = getAllProjects();

  // Migrate from old single-project format
  if (projects.length === 0) {
    try {
      const oldRaw = localStorage.getItem('bingo_state');
      if (oldRaw) {
        const old = JSON.parse(oldRaw);
        const migrated = defaultProject({
          ...old,
          id: old.id || makeId(),
          description: old.description || '',
          createdAt: Date.now(),
          updatedAt: Date.now()
        });
        _saveProjects([migrated]);
        localStorage.setItem(CURRENT_KEY, migrated.id);
        Object.assign(state, migrated);
        return migrated.id;
      }
    } catch {}
  }

  const currentId = localStorage.getItem(CURRENT_KEY);
  const current = currentId && projects.find(p => p.id === currentId);
  if (current) {
    Object.assign(state, current);
    return current.id;
  }

  if (projects.length > 0) {
    const latest = projects.reduce((a, b) => (a.updatedAt || 0) > (b.updatedAt || 0) ? a : b);
    Object.assign(state, latest);
    localStorage.setItem(CURRENT_KEY, latest.id);
    return latest.id;
  }

  return null;
}

// ── Dirty flag ────────────────────────────────────────────────────────────────
export let gridsDirty = false;
export function markGridsDirty() { if (state.grids.length > 0) gridsDirty = true; }
export function clearGridsDirty() { gridsDirty = false; }

// ── Auto-save ─────────────────────────────────────────────────────────────────
let _saveTimer = null;
export function scheduleAutoSave() {
  clearTimeout(_saveTimer);
  _saveTimer = setTimeout(saveCurrentProject, 400);
}

// ── Export / Import ───────────────────────────────────────────────────────────

async function saveFileAs(filename, content) {
  // Tauri desktop : dialog native via commande Rust
  if (window.__TAURI__?.core?.invoke) {
    try {
      await window.__TAURI__.core.invoke('save_file_dialog', { filename, content });
      return;
    } catch (e) {
      console.warn('[save] Tauri invoke failed, fallback:', e);
    }
  }
  // Android : JavascriptInterface ACTION_CREATE_DOCUMENT
  if (window.AndroidSave) {
    window.AndroidSave.saveFile(content, filename);
    return;
  }
  // Navigateur moderne : File System Access API
  if (window.showSaveFilePicker) {
    try {
      const handle = await window.showSaveFilePicker({
        suggestedName: filename,
        types: [{ description: 'JSON', accept: { 'application/json': ['.json'] } }],
      });
      const writable = await handle.createWritable();
      await writable.write(content);
      await writable.close();
      return;
    } catch (e) {
      if (e.name !== 'AbortError') console.warn('[save] showSaveFilePicker failed:', e);
      return;
    }
  }
  // Fallback : téléchargement automatique
  const blob = new Blob([content], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

export async function exportJSON() {
  const content  = JSON.stringify(state, null, 2);
  const filename = `bingo-${state.title.replace(/[^a-z0-9]/gi, '_').toLowerCase()}-${Date.now()}.json`;
  await saveFileAs(filename, content);
}

export function importJSON(json) {
  try {
    const data = JSON.parse(json);
    const p = defaultProject({ ...data, id: data.id || makeId(), updatedAt: Date.now() });
    const projects = getAllProjects();
    const idx = projects.findIndex(x => x.id === p.id);
    if (idx >= 0) projects[idx] = p;
    else projects.push(p);
    _saveProjects(projects);
    Object.assign(state, p);
    localStorage.setItem(CURRENT_KEY, p.id);
    return true;
  } catch { return false; }
}

export async function exportProjectById(id) {
  const p = getAllProjects().find(x => x.id === id);
  if (!p) return;
  const content  = JSON.stringify(p, null, 2);
  const filename = `bingo-${p.title.replace(/[^a-z0-9]/gi, '_').toLowerCase()}.json`;
  await saveFileAs(filename, content);
}

export async function exportAllProjects() {
  const projects = getAllProjects();
  const content  = JSON.stringify({ version: 1, projects }, null, 2);
  await saveFileAs(`bingo-tous-projets-${Date.now()}.json`, content);
}

export function importAllProjects(json) {
  try {
    const data = JSON.parse(json);
    let incoming = [];
    if (data && data.version === 1 && Array.isArray(data.projects)) {
      incoming = data.projects;
    } else if (Array.isArray(data)) {
      incoming = data;
    } else {
      incoming = [data];
    }
    const projects = getAllProjects();
    for (const p of incoming) {
      if (!p || typeof p !== 'object') continue;
      if (!p.id) p.id = makeId();
      const idx = projects.findIndex(x => x.id === p.id);
      if (idx >= 0) projects[idx] = p;
      else projects.push(p);
    }
    _saveProjects(projects);
    return incoming.length;
  } catch { return false; }
}
