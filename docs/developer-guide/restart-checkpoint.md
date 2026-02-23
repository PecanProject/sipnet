# Restart Checkpoint Spec

This page documents SIPNET's restart checkpoint implementation.

## Scope and Intent

The restart feature is designed to allow users and external workflows to stop the model, changing state and/or parameters. SIPNET's responsibility is:

- stop at end of climate file
- write full runtime state at segment end (`RESTART_OUT`)
- restore full runtime state at segment start (`RESTART_IN`)
- fail fast if restart context is not exactly compatible

SIPNET does not stitch outputs across segments.

## Runtime Sequence

On resume, SIPNET executes:

1. Normal setup (`setupModel`, `setupEvents`)
2. Load checkpoint and overwrite runtime state
3. Validate strict compatibility checks
4. Restore deterministic event cursor
5. Consume boundary climate row (already represented in checkpoint state)
6. Continue run from next timestep

## Schema v1.0 Overview

Checkpoint format is ASCII text with one key/value per line:

- header: `SIPNET_RESTART 1.0`
- metadata: `model_version`, `build_info`, `checkpoint_utc_epoch`, `processed_steps`
- mode flags: `flags.*`
- boundary climate signature: `boundary.*`
- deterministic event state: `event_state.*`
- mean tracker metadata: `mean.*`
- full runtime state: `envi.*`, `trackers.*`, `phenology.*`, `event_trackers.*`, `balance.*`
- mean ring buffers: `mean.values.length` + `mean.values.<idx>`, `mean.weights.length` + `mean.weights.<idx>`
- end marker: `end_restart 1`

All values are named; there are no positional value lists.

## Strict Validation Contract

On load, SIPNET enforces:

- magic header match
- schema version match
- model version/build match
- context flag compatibility
- exact first-row climate timestamp match (`year`, `day`, `time`)
- mean tracker shape/cursor validity
- deterministic event replay invariants:
  - event count
  - cursor index bounds
  - processed prefix hash
  - next-event hash/existence
- ability to restore event cursor by index

Any mismatch is a hard error. For floating-point timestamp checks, comparisons use a small tolerance suitable for text serialization.

## Climate Boundaries

The resumed climate input must start with the same timestamp (`year`, `day`, `time`) as the last processed timestep captured in the checkpoint. SIPNET validates that first timestamp, then advances the climate cursor once so the resumed segment does not reprocess the boundary timestep.

## Notes for Schema Changes

If schema contents change:

- increment schema version
- update read/write logic with explicit compatibility handling
- add integration tests for:
  - old/new schema mismatch failure
  - full continuous-vs-segmented equivalence
  - corrupted/truncated checkpoint failure modes
