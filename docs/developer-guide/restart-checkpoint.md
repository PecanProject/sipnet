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
3. Validate compatibility checks and restart boundary constraints
4. Continue run from resumed climate input

## Restart Schema v1.0 Overview

Checkpoint format is ASCII text with one key/value per line:

- header: `SIPNET_RESTART 1.0`
- metadata: `model_version`, `build_info`, `checkpoint_utc_epoch`, `processed_steps`
- schema layout guard metadata: `schema_layout.envi_size`, `schema_layout.trackers_size`, `schema_layout.phenology_trackers_size`, `schema_layout.event_trackers_size`
  - `schema_layout.trackers_size` guards the serialized tracker payload shape for schema v1.0
- mode flags: `flags.*`
- boundary metadata: `boundary.year`, `boundary.day`, `boundary.time`, `boundary.length` (no forcing fields, no cumulative GDD)
- mean tracker metadata: `mean.npp.*`
- full runtime state: `envi.*`, serialized `trackers.*`, `phenology.*`, `event_trackers.*`
  - includes `trackers.gdd` for year-to-date cumulative GDD continuity
  - excludes step-level diagnostics that are recomputed on the next timestep:
    `trackers.methane`, `trackers.nLeaching`, `trackers.nFixation`,
    `trackers.nUptake`
- mean ring buffers: `mean.npp.values.length` + `mean.npp.values.<idx>`, `mean.npp.weights.length` + `mean.npp.weights.<idx>`
- end marker: `end_restart 1`

`event_state.*` keys are not part of the schema.

Example checkpoint content is exercised in
`tests/sipnet/test_restart_infrastructure/testRestartMVP.c`.

## Validation Contract

On load, SIPNET enforces:

- magic header match
- schema version match
- model numeric version match
- `schema_layout.*` values exactly match the expected struct sizes for the running build
- build info mismatch logs warning only
- context flag compatibility
- first-row climate timestamp strictly after checkpoint boundary (`year`, `day`, `time`)
- resumed segment starts on the midnight-following day and within one timestep after midnight
- mean tracker shape/cursor validity
- `end_restart` marker value is exactly `1`
- no non-empty key/value lines appear after `end_restart`
- integer values must fit in signed 32-bit range
- floating-point values must be finite (`nan`/`inf` are rejected)
- legacy `balance.*` keys are rejected as unknown keys

All mismatches above are hard errors except build-info mismatch, which is warning-only.

## Climate and Event Boundaries

Restart writes always emit a checkpoint. If the last processed climate step is
more than one timestep before midnight, SIPNET logs a warning because that file
is useful for debugging/state inspection but should not be used for resume.

Resumed climate segments must begin on the day after the checkpoint boundary and within one timestep after midnight, where the window uses the first resumed climate row's timestep length.

Event files must be segmented to the same time boundaries as climate segments.

## When Saved State Changes

If you add saved state or change an existing saved payload:

1. Update the serialized payload type and restart read/write logic in `src/sipnet/restart.c`.
2. Update the `RESTART_SCHEMA_LAYOUT_*` constants, static asserts, and runtime schema-layout validation.
3. Update restart docs/tests and bump `RESTART_SCHEMA_VERSION`.

## Struct Drift Guards

Restart schema v1.0 includes compile-time and runtime drift guards so struct layout changes cannot silently pass:

- Compile-time guards: `_Static_assert` checks in `src/sipnet/restart.c` for `Envi`, serialized tracker payload shape, `PhenologyTrackers`, and `EventTrackers`.
- Runtime guards: `schema_layout.*` fields in each checkpoint are validated on load.
- Test guardrails: `tests/sipnet/test_restart_infrastructure/testRestartMVP.c` verifies schema layout keys are present and rejects tampered values.

## Schema Bump Checklist

When intentionally changing the restart schema version:

1. Update `src/sipnet/restart.c` in all schema touchpoints: `RESTART_SCHEMA_VERSION`, `RESTART_SCHEMA_LAYOUT_*`, `_Static_assert` layout guards, and checkpoint read/write + required-key validation logic.
2. Update restart examples/fixtures to the new header and key set, including the restart fixtures in `tests/sipnet/test_restart_infrastructure/testRestartMVP.c`.
3. Update docs that name schema version or key expectations: `docs/developer-guide/restart-checkpoint.md` and `docs/user-guide/running-sipnet.md`.
