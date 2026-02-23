# Restart Checkpoint Spec (MVP)

This page documents SIPNET's restart checkpoint implementation contract for developers.

## Scope and Intent

The restart feature is designed for segmented orchestration (for example, SDA-style external workflow control). SIPNET's responsibility is:

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

Checkpoint file format is portable ASCII text, schema `1.0`, with deterministic field order.

Top-level records include:

- header:
  - `SIPNET_RESTART_ASCII 1.0`
  - `model_version`
  - `build_info`
  - `checkpoint_utc_epoch`
  - `processed_steps`
- model-mode flags:
  - `flags` (events/gdd/growthResp/leafWater/litterPool/snow/soilPhenol/waterHResp/nitrogenCycle/anaerobic)
- boundary and deterministic restart metadata:
  - `boundary` climate signature (last processed row)
  - `event_state` invariants (cursor/count/prefix hash/next-event hash)
  - `mean_meta` (length/totWeight/start/last/sum)
- full runtime state blocks:
  - `envi`
  - `trackers`
  - `phenology`
  - `event_trackers`
  - `balance`
  - `mean_values`
  - `mean_weights`
  - `end_restart`

Model parameters from `.param` are intentionally not serialized in restart files.

## Strict Validation Contract

On load, SIPNET enforces:

- magic header match
- schema version match
- model version/build match
- context flag compatibility
- exact first-row climate boundary match
- mean tracker shape/cursor validity
- deterministic event replay invariants:
  - event count
  - cursor index bounds
  - processed prefix hash
  - next-event hash/existence
- ability to restore event cursor by index

Any mismatch is a hard error. For floating-point boundary checks, comparisons use a small tolerance suitable for text serialization.

## Boundary Semantics

MVP boundary semantics require that resumed climate input starts with the same row as the last processed row captured in the checkpoint. SIPNET validates that first row, then advances the climate cursor once so the resumed segment does not reprocess the boundary timestep.

## Notes for Schema Changes

If schema contents change:

- increment schema version
- update read/write logic with explicit compatibility handling
- add integration tests for:
  - old/new schema mismatch failure
  - full continuous-vs-segmented equivalence
  - corrupted/truncated checkpoint failure modes
