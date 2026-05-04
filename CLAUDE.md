# open_bingo — CLAUDE.md

## Project overview

A browser-based bingo card generator for tabletop game nights. Players get unique randomized grids based on a shared pool of themed cells. The app runs locally via a minimal Express static server.

## Stack

- **Backend**: Node.js + Express (static file server only, no API)
- **Frontend**: Vanilla JS ES modules, no build step
- **Styling**: Plain CSS (4 files: main, dashboard, grid, print)
- **Entry**: `bingo-app/server.js` → serves `bingo-app/public/`

## Running the app

```bash
cd bingo-app
npm install
npm start       # http://localhost:3000
```

## Architecture

```
bingo-app/public/
  index.html          # Single-page shell with 4 tabs
  js/
    state.js          # Central mutable state + JSON export/import
    main.js           # Tab routing, file I/O wiring
    config.js         # Tab: title, grid size, HP, multipliers, players
    cases.js          # Tab: manage bingo cells (label, points, rate%)
    generator.js      # Grid generation logic (rate-based shuffle)
    grids.js          # Tab: display generated grids per player
    print.js          # Tab: print preview
    ui.js             # Shared helpers (showToast)
  style/
    main.css          # Global layout, header, tabs
    dashboard.css     # Config form, player rows
    grid.css          # Grid table rendering
    print.css         # @media print rules
```

## State shape

All app state lives in a single object exported from `state.js`:

```js
{
  title: string,
  gridSize: number,          // N for N×N grid
  players: [{ name }],
  startHP: number,
  freeCenter: boolean,       // center cell FREE if N is odd
  multipliers: { line, column, diagonal, full },
  cases: [{ label, points, rate }],   // rate = 0-100 inclusion probability
  gages: [{ description, hp }],
  grids: [{ player, cells[][] }]      // generated, reset on config save
}
```

## Key behaviors

- **Rate-based generation**: each cell has a `rate` (0–100%). During generation, cells are included if `Math.random()*100 < rate`. Grids are padded/cycled if the pool is too small.
- **Config save resets grids**: changing config wipes `state.grids` to avoid stale data.
- **No persistence**: state is in-memory. Export/import JSON manually via the UI buttons.

## Dev conventions

- No framework, no bundler — just native ES modules. Keep it that way.
- All rendering is string-based `innerHTML` — escape user input with the `esc()` helper in `config.js`.
- Do not add a backend API. All logic stays client-side.
