# Restart Checkpoint Spec

This page documents SIPNET's restart checkpoint implementation.

## Scope and Intent

SIPNET restart is designed for segmented orchestration:

- stop at end of one climate segment
- write full runtime state at segment end (`RESTART_OUT`)
- restore full runtime state at next segment start (`RESTART_IN`)
- fail fast on incompatible restart/configuration inputs

SIPNET itself does not stitch outputs across segments.

## Runtime Sequence

On resume, SIPNET executes:

1. Normal setup (`setupModel`, `setupEvents`)
2. Load checkpoint and overwrite runtime state
3. Validate compatibility checks and restart boundary checks
4. Continue run from resumed climate input

## Restart Schema v1.0 Overview

Checkpoint format is ASCII text with one key/value per line:

- header: `SIPNET_RESTART 1.0`
- metadata: `meta_info.model_version`, `meta_info.build_info`, `meta_info.checkpoint_utc_epoch`, `meta_info.processed_steps`
- schema layout guard metadata: `schema_layout.envi_size`, `schema_layout.trackers_size`, `schema_layout.phenology_trackers_size`, `schema_layout.event_trackers_size`
- mode flags: `flags.*`
- boundary metadata: `boundary.year`, `boundary.day`, `boundary.time`, `boundary.length`
- mean tracker metadata: `mean.npp.*`
- full runtime state: `envi.*`, `trackers.*`, `phenology.*`, `event_trackers.*`
- mean ring buffers: `mean.npp.values.<idx>`, `mean.npp.weights.<idx>`
- end marker: `end_restart 1`

Example checkpoint content is exercised in
`tests/sipnet/test_restart_infrastructure/testRestartMVP.c`.

## Validation Contract

On load, SIPNET enforces the following. Lines that start with (warning) log a warning and do not error.

- magic header match
- schema version match
- model numeric version match
- `schema_layout.*` values exactly match the expected struct sizes for the running build
- (warning) build info mismatch 
- context flag compatibility
- first-row climate timestamp strictly after checkpoint boundary (`year`, `day`, `time`)
- (warning) resumed segment starts on the midnight-following day and within one timestep after midnight
- mean tracker shape/cursor validity
- All lines appearing after `end_restart` are ignored
- integer values must fit in signed 32-bit range
- floating-point values must be finite (`nan`/`inf` are rejected)

All mismatches above are hard errors except as indicated.

## Climate and Event Boundaries

Restart writes always emit a checkpoint. If the last processed climate step is
more than one timestep before midnight, SIPNET logs a warning, as there will be a time gap in any resumption from that 
file.

Resumed climate segments must begin on the day after the checkpoint boundary. If they start more than one timestep 
after midnight (using the first resumed climate row's timestep length) SIPNET logs a warning.

Event files must be segmented to the same time boundaries as climate segments.

## When Saved State Changes

If you add saved state or change an existing saved payload:

1. Update the serialized payload type and restart read/write logic in `src/sipnet/restart.c`.
2. Update the `RESTART_SCHEMA_LAYOUT_*` constants, static asserts, and runtime schema-layout validation.
3. Update restart docs/tests and bump `RESTART_SCHEMA_VERSION`.

## Struct Drift Guards

Restart schema v1.0 includes compile-time and runtime drift guards so struct layout changes cannot silently pass:

- Compile-time guards: `_Static_assert` checks in `src/sipnet/restart.c` for `Envi`, `Trackers`, `PhenologyTrackers`, `EventTrackers`, and expected number of model flags in `Context`.
- Runtime guards: `schema_layout.*` fields in each checkpoint are validated on load.
- Test guardrails: `tests/sipnet/test_restart_infrastructure/testRestartMVP.c` verifies schema layout keys are present and rejects tampered values.

## Schema Bump Checklist

When intentionally changing the restart schema version:

1. Update `src/sipnet/restart.c` in all schema touchpoints: `RESTART_SCHEMA_VERSION`, `RESTART_SCHEMA_LAYOUT_*`, `_Static_assert` layout guards, and checkpoint read/write + required-key validation logic.
2. Update restart examples/fixtures to the new header and key set, including the restart fixtures in `tests/sipnet/test_restart_infrastructure/testRestartMVP.c`.
3. Update docs that name schema version or key expectations: `docs/developer-guide/restart-checkpoint.md` and `docs/user-guide/running-sipnet.md`.
