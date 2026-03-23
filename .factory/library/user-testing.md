# User Testing

Testing surface, required tools, resource cost classification per surface.

**What belongs here:** How to validate the game through its user surface, testing tools, resource constraints.

---

## Validation Surface

- **Primary surface**: Browser at http://localhost:5173
- **Tool**: agent-browser (Chrome DevTools MCP)
- **Interaction methods**:
  - Keyboard shortcuts: WASD (movement), 1-6 (skills), TAB (auto-combat), I/K/M/H/C/J/O/P (panels), ESC (menu)
  - Coordinate-based mouse clicks on Phaser canvas (canvas is opaque to a11y tree)
  - `evaluate_script` for querying game state via `window.game` (if exposed)
- **Limitations**:
  - Canvas is opaque — a11y snapshot only shows root element
  - All verification requires screenshot analysis + keyboard-driven interaction
  - Button positions must be estimated from screenshots (fragile on layout changes)

## Validation Concurrency

- **Max concurrent validators**: 2
- **Rationale**: Phaser game loop uses ~105% CPU per instance on 8-core machine. 16GB RAM with ~6GB baseline. Each browser instance ~500MB. 2 instances = ~1GB additional = safe within 70% of 10GB headroom.
- **Dev server**: Single Vite server at port 5173, shared across validator instances

## Testing Infrastructure

- **Unit tests**: Vitest for pure logic modules (CombatSystem, PathfindingSystem, LootSystem, etc.)
- **Build verification**: `npm run build` (tsc + vite build) as quality gate
- **No e2e framework**: Visual validation via agent-browser screenshots only
- **Phaser mocking**: Tests must mock Phaser dependencies to run in Node environment

## Dev Server Management

- Start: `npx vite --port 5173`
- Healthcheck: `curl -sf http://localhost:5173`
- The dev server may already be running from user's terminal
