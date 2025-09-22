# Code Structure

This guide documents how state is advanced each timestep and the conventions that keep flux calculations pure and pool updates centralized.

## Timestep Phases (in `updateState()`)

1) Initialize fluxes
- Zero all `fluxes.*` and `fluxes.event*`.

2) Compute fluxes (pure calculations)
- `calculateFluxes()` computes photosynthesis, respiration, water/snow, etc.
- `processEvents()` converts scheduled/instant events to `fluxes.event*` deltas (no pool mutation).
- `soilDegradation()` and other biogeochemical modules compute additional flux rates only (no pool mutation).
- No function in this phase mutates `envi.*` or `trackers.*`.

3) Apply pool updates (single place)
- `applyPoolUpdates()` is the only code that changes `envi.*`.
- For each pool P: ΔP = (sum of rate fluxes to P) * climate.length + (sum of `fluxes.event*` deltas to P).
- Apply bounds, conservation, and cross-pool constraints here.

4) Trackers and running means
- `updateTrackers()` uses timestep-integrated values (rate * climate.length) plus event deltas.

5) Output
- `outputState()` and any optional diagnostics/logging.

### Pseudocode outline

- updateState():
  - zeroFluxes()
  - calculateFluxes()        // pure rates
  - processEvents()          // sets fluxes.event* deltas only
  - soilDegradation()        // pure rates
  - applyPoolUpdates()       // the only place that mutates envi.*
  - updateTrackers()
  - outputState()

## Mutability Rules (must-follow)

- Only `applyPoolUpdates()` may change `envi.*`.
- Flux calculators:
  - May read `envi.*`, `params.*`, `ctx.*`, `climate.*`.
  - May write `fluxes.*` (rates) and `fluxes.event*` (event deltas).
  - Must not mutate `envi.*`, `trackers.*`, or perform I/O as logic side-effects.
- Events never change pools directly; they only add to `fluxes.event*`.

## Units and Integration

- Rate fluxes in `fluxes.*` are per-day rates (pool units per day).
- Event deltas in `fluxes.event*` are direct pool deltas (same units as pools), not rates.
- Integration per pool each timestep:
  - Δpool_from_rates = (sum of relevant `fluxes.*`) * climate.length
  - Δpool_from_events = (sum of relevant `fluxes.event*`)
  - pool += Δpool_from_rates + Δpool_from_events

## Naming Conventions

- envi.*        State variables (pools, water, snow, canopy, soil layers).
- fluxes.*      Per-day flux rates computed in the flux phase.
- fluxes.event* Instantaneous/event deltas to be applied during pool update.
- trackers.*    Integrated timestep values, cumulative sums, yearly aggregates.
- params.*      Fixed run parameters (immutable during a run).
- ctx.*         Feature flags / configuration switches.
- climate.*     Forcing for the current timestep (e.g., length, met drivers).
- diag.*        Optional transient diagnostics (no side effects on state).

Name fluxes by direction and target, e.g., `fluxes.NPP`, `fluxes.soilRespiration`, `fluxes.leafLitterToSoil`, `fluxes.eventHarvestC`. Prefer “to/from” clarity for transfers.

## Pool Update Responsibilities

- Apply all additions/removals in a consistent order if constraints require it (e.g., water first if it bounds biochemical rates next step).
- Enforce invariants:
  - No negative pools; clamp with tracked deficits and warnings if needed.
  - Mass conservation across linked pools (e.g., C/N stoichiometry) with balanced cross-pool transfers.
- Centralize any event-specific application here (e.g., harvest removing biomass, adding residues).

## Adding a New Flux or Event

- Rates: add a `fluxes.*` variable, compute it in a flux function, and integrate it in `applyPoolUpdates()`.
- Events: add a `fluxes.event*` delta, accumulate in `processEvents()`, apply it in `applyPoolUpdates()`.
- Do not mutate `envi.*` in calculators or event processors.

## Logging & Errors

- Use `logError()` and `logWarning()` (not printf) so tests can capture output.
- Messages should include timestep context: year, day, event type (if relevant), and the offending value(s).
- Emit warnings on clamping, conservation corrections, or unexpected negative fluxes.