AGENTS: Ignore this file unless explicitly asked to edit it; when running sessions, focus only on the pasted session prompt and do not infer tasks from this file.

# PR‑276 Restart Refactor — Codex Orchestration Canvas

## Global Requirements

- You are implementing maintainer feedback on PR #276 (“SIP279 SIPNET Restart MVP”).
- Follow this implementation contract strictly.

### Hard Requirements
- Climate segments that write restart checkpoints **must** end within one time step of midnight 
- Restart compatibility policy:
  - ERROR on restart schema version mismatch.
  - ERROR on model numeric version mismatch.
  - WARN (do not fail) on build-info mismatch.
- Events must be segmented to have same time boundaries as climate.
- Remove event cursor/hash resume logic.
- Store cumulative GDD in tracker state, not `ClimateVars`. Leave current climate GDD in `ClimateVars`.
- Restart parser must tolerate missing final newline.
- Restart parameters must be included in config dump output.
- Add serialization drift guards so struct changes cannot silently break restart.
- Only focus on the prompt given to you directly, do not attempt to implement future steps or refactor beyond the scope of the current step.
- Do not bump the schema or model version

### Sessions Checklist

- [ ] Session 1 — CLI / Config
  - [x] Step 0 — Remove Strict Flag, Add Events File, Fix Config Logging
- [ ] Session 2 — Restart Parser + Version Policy
  - [ ] Step 1 — Restart Parser Robustness
  - [ ] Step 2 — Relax Build Info Strictness
- [ ] Session 3 — Runtime Boundaries + Events + GDD
  - [ ] Step 3 — Enforce Midnight Climate Boundary
  - [ ] Step 4 — Segmented Events & Remove Cursor/Hash Resume Logic
  - [ ] Step 5 — Move Cumulative GDD from ClimateVars to Trackers
- [ ] Session 4 — Schema/Serialization Changes
  - [ ] Step 6 — Serialization Drift Guards
  - [ ] Step 7 — Boundary Metadata Reduction
  - [ ] Step 8 — Mean Tracker Collection Rename
  - [ ] Step 9 — Drop Balance State from Restart
- [ ] Session 5 — Documentation
  - [ ] Step 10 — Documentation Sweep

Do not refactor beyond scope. Preserve MVP simplicity and maintain deterministic restart equivalence.

## Session 1 — CLI / Config

### Session 1 Prompt
Implement Step 0 only. Start with Step 0 and stop after it is completed. Summarize what has been done and do not move on to any other step until I have told you to commit changes.
Global Requirements (apply to this session):
- Follow the implementation contract strictly.
- Only focus on this step; do not refactor beyond scope or touch future steps.
- Do not bump the schema or model version.
Relevant Hard Requirements:
- Restart compatibility policy must be enforced (schema/model errors, build warning).
- Restart parameters must be included in config dump output.

### Session 1 Checklist
- [x] Step 0 — Remove Strict Flag, Add Events File, Fix Config Logging

### Step 0 - Remove Strict Flag, Add Events File, Fix Config Logging

#### Goal
- Remove --restart-strict.
- Always enforce the version policy.
- Add --events-file with default events to preserve current behavior.
- Include restart keys in config dump.

#### Key Files
- cli.c
- cli.h
- events.c
- context.c
- context.h
- Docs (user + developer guides)

#### Required Behavior
- Remove RESTART_STRICT.
- Add EVENTS_FILE CLI/config option.
- printConfig must include RESTART_IN, RESTART_OUT, and EVENTS_FILE.

#### Version Enforcement Policy
- Schema mismatch → ERROR.
- Model numeric version mismatch → ERROR.
- Build info mismatch → WARNING only.

#### Acceptance Criteria
- Restart runs without the strict flag.
- Config dump includes restart parameters.
- EVENTS_FILE parameter exists and defaults to events.
- Tests updated accordingly.

#### Status

Committed dfbd7467 updates restart/config behavior for Step 0:

Removed --restart-strict / RESTART_STRICT support.
Added --events-file <name> and new config key EVENTS_FILE (default events), so event input resolves as <EVENTS_FILE>.in (default events.in).
Updated config dumping so restart-related keys are included (including RESTART_IN, RESTART_OUT, and EVENTS_FILE).
Changed restart enforcement policy so:
schema mismatch = error
numeric model version mismatch = error
build info mismatch = warning only
Updated restart infrastructure test inputs to remove RESTART_STRICT.
Updated restart MVP test expectation to schema header SIPNET_RESTART 1.0.
Updated docs in:
running-sipnet.md
restart-checkpoint.md

## Session 2 — Restart Parser + Version Policy

### Session 2 Prompt
Implement Step 1 and Step 2 in order. Start with Step 1 and stop after it is completed. Summarize what has been done and do not move on to Step 2 until I have told you to commit changes.
Global Requirements (apply to this session):
- Follow the implementation contract strictly.
- Only focus on these steps; do not refactor beyond scope or touch future steps.
- Do not bump the schema or model version.
Relevant Hard Requirements:
- Restart parser must tolerate missing final newline.
- Restart compatibility policy must be enforced (schema/model errors, build warning).

### Session 2 Checklist
- [x] Step 1 — Restart Parser Robustness
- [ ] Step 2 — Relax Build Info Strictness

### Step 1 — Restart Parser Robustness
- Model: GPT‑5.2‑Codex
- Reasoning Mode: High
- Parallel Safe With: Step-0

#### Goal
- Accept restart files missing a final newline.
- Use correct exit codes for validation failures.

#### Key Files
- `src/sipnet/restart.c`
- Restart tests

#### Acceptance Criteria
- EOF without trailing `\n` loads successfully.
- Truly truncated files still fail.
- Validation errors use `EXIT_CODE_BAD_PARAMETER_VALUE`.

#### Status

Completed (uncommitted in this branch):
- Restart parser now accepts EOF without trailing newline while still rejecting truly truncated/overlong lines.
- Restart parse/value validation failures now use `EXIT_CODE_BAD_PARAMETER_VALUE`.
- Restart infrastructure tests now cover missing final newline success and assert `EXIT_CODE_BAD_PARAMETER_VALUE` for validation failures.

### Step 2 — Relax Build Info Strictness
- Model: GPT‑5.3‑Codex
- Reasoning Mode: Medium
- Run After: Step-1

#### Goal
- Build mismatch = warning.
- Model numeric mismatch = error.
- Schema mismatch = error.

#### Key Files
- `src/sipnet/restart.c`
- Tests
- Developer docs

#### Acceptance Criteria
- Same schema/model + different build → restart succeeds with warning.
- Different schema/model → restart fails with error.

## Session 3 — Runtime Boundaries + Events + GDD

### Session 3 Prompt
Implement Step 3 through Step 5 in order. Start with Step 3 and stop after it is completed. Summarize what has been done and do not move on to Step 4 until I have told you to commit changes.
Global Requirements (apply to this session):
- Follow the implementation contract strictly.
- Only focus on these steps; do not refactor beyond scope or touch future steps.
- Do not bump the schema or model version.
Relevant Hard Requirements:
- Climate segments that write restart checkpoints must end within one time step of midnight.
- Events must be segmented to have same time boundaries as climate.
- Remove event cursor/hash resume logic.
- Store cumulative GDD in tracker state, not ClimateVars (leave current climate GDD in ClimateVars).

### Session 3 Checklist
- [ ] Step 3 — Enforce Midnight Climate Boundary
- [ ] Step 4 — Segmented Events & Remove Cursor/Hash Resume Logic
- [ ] Step 5 — Move Cumulative GDD from ClimateVars to Trackers

### Step 3 — Enforce Midnight Climate Boundary (Hard Requirement)
- Model: GPT‑5.3‑Codex
- Reasoning Mode: High
- Run After: Step-2

#### Goal
- Require checkpoint writes only within (<=) one time step before midnight.
- Require restart runs to start within (<=) one time step after the last midnight checkpoint.

#### Key Files
- `src/sipnet/restart.c`
- Possibly `sipnet.c`
- Test `.clim` fixtures
- Docs

#### Behavior
- Fatal error if a segment ends more than one time step before midnight.
- Fatal error if a restart run starts more than one time step after the last midnight checkpoint.
- Update all fixtures to comply.

#### Acceptance Criteria

- Implemented as described.
- All tests pass with updated fixtures.

### Step 4 — Segmented Events & Remove Cursor/Hash Resume Logic
- Model: GPT‑5.3‑Codex
- Reasoning Mode: Extra High
- Run After: C‑01 + Step-3

#### Goal
- Remove event hashing/cursor resume machinery.
- Drop `event_state.*` from the restart schema.
- Rely on segmented events files.

Now that restarts must stop and start within one time step of midnight, our current event validation will prevent unexpected behavior at the boundary. We can remove the complexity of event cursors and hashes, and simply require that events be segmented with the same time boundaries as climate segments.

#### Key Files
- `src/sipnet/events.c`
- `src/sipnet/events.h`
- `src/sipnet/restart.c`
- `src/sipnet/sipnet.c`
- Tests
- Docs

#### Acceptance Criteria
- No event cursor/hash logic remains.
- Restart file has no `event_state` section.
- Segmented runs equal continuous runs.
- No event replay at boundaries.

#### Status

Not completed

### Step 5 — Move Cumulative GDD from ClimateVars to Trackers
- Model: GPT‑5.3‑Codex
- Reasoning Mode: Extra High
- Run After: Step-4

#### Goal

- Remove the `gdd` from `ClimateVars`.
- Store and update Cumulative GDD in the tracker struct.
- Reset via lastYear logic.
- Serialize under the tracker section only.

#### Key Files
- `src/sipnet/state.h`
- `src/sipnet/sipnet.c`
- `src/sipnet/restart.c`
- Phenology logic
- Tests
- Docs

#### Acceptance Criteria

- `trackers.gdd` serialized/restored.
- Continuous vs. segmented runs identical.

#### Status

Not completed

## Session 4 — Schema/Serialization Changes

### Session 4 Prompt
Implement Step 6 through Step 9 in order. Start with Step 6 and stop after it is completed. Summarize what has been done and do not move on to Step 7 until I have told you to commit changes.
Global Requirements (apply to this session):
- Follow the implementation contract strictly.
- Only focus on these steps; do not refactor beyond scope or touch future steps.
- Do not bump the schema or model version.
Relevant Hard Requirements:
- Add serialization drift guards so struct changes cannot silently break restart.

### Session 4 Checklist
- [ ] Step 6 — Serialization Drift Guards
- [ ] Step 7 — Boundary Metadata Reduction
- [ ] Step 8 — Mean Tracker Collection Rename
- [ ] Step 9 — Drop Balance State from Restart

### Step 6 — Serialization Drift Guards
- Model: GPT‑5.3‑Codex
- Reasoning Mode: High
- Run After: Step-5

#### Goal
- Prevent silent restart breakage when structs change.

#### Approach
- Add `sizeof` or field-count checks.
- Tie to the restart schema version constant.
- Add a unit test.

#### Acceptance Criteria
- Changing `ENVI` or tracker structs (trackers, phenology, event_trackers) without a schema bump fails CI.

#### Status

Not completed

### Step 7 — Boundary Metadata Reduction
- Model: GPT‑5.3‑Codex
- Reasoning Mode: Medium
- Run After: Step-6

#### Goal
- Keep only `boundary.year`, `boundary.day`, `boundary.time`, and `boundary.length` in the restart schema.
- Remove all other `boundary.*` lines from the restart file and ensure docs/tests reference the trimmed format.

#### Key Files
- `src/sipnet/restart.c`
- `docs/developer-guide/restart-checkpoint.md`
- `docs/developer-guide/sipnet.restart.example`
- Restart tests/version checking fixtures

#### Acceptance Criteria
- Restart I/O still validates, but files only contain the four boundary fields.
- Documentation and examples match the lean boundary metadata.

#### Status

Not completed

### Step 8 — Mean Tracker Collection Rename
- Model: GPT‑5.3‑Codex
- Reasoning Mode: Medium
- Run After: Step-7

#### Goal
- Rename every `mean.*` key to `mean.npp.*` so the metric namespace matches the tracker it represents.
- Move the entire `mean.npp.*` block to the very end of the restart file so all runtime data precedes it and `end_restart` directly follows.

#### Key Files
- `src/sipnet/restart.c`
- `docs/developer-guide/restart-checkpoint.md`
- `docs/developer-guide/sipnet.restart.example`
- Restart tests/fixtures that inspect output ordering

#### Acceptance Criteria
- Restart parser emits and accepts `mean.npp.*` entries only.
- Restart files consistently place the `mean.npp.*` block right before `end_restart 1` in recorded samples.

#### Status

Not completed


### Step 9 — Drop Balance State from Restart
- Model: GPT‑5.3‑Codex
- Reasoning Mode: Medium
- Run After: Step-8

#### Goal
- Remove serialization/deserialization of `balance.*` entirely from the restart schema and code.
- Ensure the runtime no longer expects or requires balance state when resuming.

#### Key Files
- `src/sipnet/restart.c`
- `src/sipnet/sipnet.c` (if needed for rehydration behavior)
- `docs/developer-guide/restart-checkpoint.md`
- `docs/developer-guide/sipnet.restart.example`
- Restart tests/fixtures that validate balance entries

#### Acceptance Criteria
- Restart files no longer contain any `balance.*` lines.
- Code updates still produce deterministic restarts without referencing balance fields.

#### Status

Not completed

## Session 5 — Documentation

### Session 5 Prompt
Implement Step 10 only after all prior steps are complete. Start with Step 10 and stop after it is completed. Summarize what has been done and do not move on to any other step until I have told you to commit changes.
Global Requirements (apply to this session):
- Follow the implementation contract strictly.
- Only focus on this step; do not refactor beyond scope or touch future steps.
- Do not bump the schema or model version.

### Session 5 Checklist
- [ ] Step 10 — Documentation Sweep

### Step 10 — Documentation Sweep
- Model: GPT‑5.3‑Codex
- Reasoning Mode: Low
- Run Last

#### Update Docs To Reflect
- Midnight boundary requirement.
- Segmented events required.
- Build mismatch = warning.
- GDD under trackers.
- Struct drift guard workflow.
