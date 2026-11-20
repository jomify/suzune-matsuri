# Story, Health and Death Replay Implementation Plan

> **For agentic workers:** Execute the bounded tasks below in this session with native subagents; the leader owns integration, builds and verification. Do not recursively delegate or revert another contributor's edits.

**Goal:** Deliver the confirmed redemption story, reliable boss damage, forgiving HP and an actual short death replay.

**Architecture:** Pure health/story/replay-buffer data feed the existing subsystem and code-built UMG. GameMode coordinates damage and replay; centralized bullet rendering reuses captured bullet state.

**Tech Stack:** Existing UE 5.8 C++, UMG and UE Automation; no new dependencies.

## 1. Health rules and HUD

- [ ] Extend pure run-state tests for 200/250 HP, positive damage, immunity blessing, fatal clamp, heal and +50 health charm. Update lives-based expectations to HP.
- [ ] Implement `Hurt(float Damage = 20.0f)`, `Heal(float Amount)`, `GetMaxHealth()`, `Health`; subsystem forwards damage/heal and broadcasts accepted changes.
- [ ] Replace ExtraLives with ExtraHealth, update health charm descriptions, HUD HP bar and game-over labels. Preserve save record format.
- [ ] Leader adapts GameMode: short grace instead of respawn; fatal handoff; floor refill. Run health and legacy gameplay tests after compiling.

## 2. Story

- [ ] Add pure chapter/ending data and tests for all heroes/floors and the guardian ending.
- [ ] Add a code-built skippable dialogue widget. API: `UMatsuriStoryWidget::Show(APlayerController&, const FMatsuriStoryChapter&, FSimpleDelegate)`; `Finish()` is idempotent and restores prior pause state.
- [ ] Leader integrates chapter display at floor entry and ending before victory. Expose `matsuri.Story` and disable it only in unrelated automation fixtures.

## 3. Boss damage

- [ ] Write regressions using existing bullet and PIE harness APIs. Run failing regressions against baseline before production changes.
- [ ] Add swept bullet/player and cover tests, preserve first impact, split touch accounting from hit acceptance, enforce telegraph timing and avoid damage callbacks inside array iteration.
- [ ] Expose bullet capture/render hooks for replay. Source and skill travel with fatal damage; leader integrates GameMode acceptance and replay.

## 4. Replay and integration

- [ ] Add a bounded six-second history of timestamped actor transforms and boss bullets; pure tests cover truncation and sample selection.
- [ ] Record during active gameplay; on fatal hit freeze world and use a replay camera, captured actors/projectiles, heroine proxy and skip overlay. Finish invokes normal final UI once.
- [ ] Add PIE tests for historical motion, no HP/score/Boss-memory side effects, completion/skip and insufficient history.
- [ ] Inspect rendered HP/dialogue/replay screenshots before the next visual revision; record visual verdict under `.omx/state/`.

## 5. Verification and delivery

- [ ] Compile with `tools/build.sh`, using the bundled engine toolchain; trigger UBT discovery after adding C++ files.
- [ ] Run targeted tests via `tools/ue_cmd.sh` with `-nullrhi`, then the entire `SuzuneMatsuri` suite.
- [ ] Run a rendered story/health/replay smoke test and inspect screenshots and runtime errors.
- [ ] Obtain C++ review, resolve actionable findings, update README/logbook and commit only project changes using Lore messages. Keep known limits explicit.
