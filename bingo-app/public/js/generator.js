import { state } from './state.js';

function shuffle(arr) {
  const a = [...arr];
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}

export function calcRequirements() {
  const N = state.gridSize;
  const total = N * N;
  const hasCenter = state.freeCenter && N % 2 === 1;
  const available = hasCenter ? total - 1 : total;
  const players = state.players.length;
  return { N, total, hasCenter, available, players, minCases: available };
}

function buildGrid(playerName, cellData, N, hasCenter) {
  const centerRow = Math.floor(N / 2);
  const centerCol = Math.floor(N / 2);
  let idx = 0;
  const cells = [];

  for (let r = 0; r < N; r++) {
    const row = [];
    for (let c = 0; c < N; c++) {
      if (hasCenter && r === centerRow && c === centerCol) {
        row.push({ label: 'FREE', points: 0, rate: 100, gage: '', gageHP: 0, isFree: true });
      } else {
        row.push({ ...cellData[idx++] });
      }
    }
    cells.push(row);
  }

  return { player: playerName, cells };
}

function generatePlayerGrid(playerName, N, hasCenter, available) {
  const included = [];
  const excluded = [];

  for (const c of state.cases) {
    const rate = c.rate ?? 50;
    if (Math.random() * 100 < rate) {
      included.push({ ...c });
    } else {
      excluded.push({ ...c });
    }
  }

  // Fill to exactly `available` slots: prefer included, pad with excluded, then cycle if needed
  const shuffledIncluded = shuffle(included);
  const shuffledExcluded = shuffle(excluded);

  let cells;
  if (shuffledIncluded.length >= available) {
    cells = shuffledIncluded.slice(0, available);
  } else {
    cells = [...shuffledIncluded, ...shuffledExcluded.slice(0, available - shuffledIncluded.length)];
  }

  // If still not enough (fewer phrases than cells), cycle through all phrases
  if (cells.length < available) {
    const pool = shuffle([...state.cases]);
    let i = cells.length;
    while (cells.length < available) {
      cells.push({ ...pool[i % pool.length] });
      i++;
    }
  }

  return buildGrid(playerName, shuffle(cells), N, hasCenter);
}

export function generateAll() {
  const { N, hasCenter, available, players } = calcRequirements();

  if (players === 0) {
    return { error: true, message: 'Ajoutez au moins un joueur dans la configuration.' };
  }

  if (state.cases.length === 0) {
    return { error: true, message: 'Ajoutez au moins une case dans l\'onglet Cases.' };
  }

  state.grids = state.players.map((player) =>
    generatePlayerGrid(player.name, N, hasCenter, available)
  );

  const repeats = state.cases.length < available;
  return { error: false, repeats };
}

export function reshuffleGrid(playerIdx) {
  const { N, hasCenter, available } = calcRequirements();
  const grid = state.grids[playerIdx];
  if (!grid) return;
  state.grids[playerIdx] = generatePlayerGrid(grid.player, N, hasCenter, available);
}
