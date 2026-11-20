# Night festival story, health and kill replay

## Confirmed direction

The player chose the redemption story: scattered wish coins destabilize the
festival barrier. Sakura, Suzune or Mio follows the trail through three floors.
Tamamo has sealed the festival to protect the villagers; her final danmaku
trial gathers the wishes and repairs the barrier with the heroine.
The player also chose: zero HP -> death replay -> game over -> fresh run.

## Player-facing behavior

- A skippable chapter dialogue on entering each floor introduces the heroine,
  advances the mystery and gives the collection objective; the boss chapter
  reveals Tamamo's protective purpose. Victory concludes the same story.
- Replace discrete lives and teleport-on-every-hit with HP. Base maximum 200,
  Sakura 250. Ordinary hazards/contact 20 damage, boss bullets 8 damage.
  A positive nonfatal hit stays in place and grants a short 0.35-second grace
  window; barrier and fox blessing remain distinct protection mechanics.
  Health omamori increases maximum HP by 50 and heals 50, bounded at 600 HP.
  Each new floor restores HP. Incoming damage does not remove coin score.
- HUD shows current/max HP and a health bar, with hurt/protection feedback.
- Fatal damage records the culprit and skill, then freezes real combat and
  shows up to the last six seconds from the culprit's view. Captured player,
  enemies and boss bullets replay past motion; it is not live spectating.
  Show a heroine proxy so the victim is visible from the killer's view.
  Replay has a clear label/progress and a skip button. Completion/skip opens
  normal game over once. With insufficient history or no identifiable killer,
  use the available history and a labelled victim/orbit perspective; time-out
  has no killer and goes directly to its existing game-over screen.
- Replay does not apply damage, collect coins, advance the timer, retrigger
  game results or write Boss rewards. Ring buffer and bullet budget bound
  memory; no video capture or new dependency is introduced.

## Damage correctness

Use continuous segment tests for player and cover collisions, including player
motion between samples. Preserve cover-before-player ordering on that segment.
Do not use the once-per-bullet touch statistic as permission to damage. Respect
telegraph delay, actual invulnerability at impact and the tiny player hit sphere.
Report actual accepted damage for hit statistics. Dispatch damage after bullet
iteration so fatal cleanup cannot invalidate the array; the fatal frame must
be captured before cleanup. Keep all six skill types subject to the same rules.

## Boundaries and integration

Health rules stay in the pure run state and broadcast through the existing run
subsystem. Story data and code-built UMG are separate from combat. A replay
director records the current world, owns a replay camera/overlay, and drives
recorded visual state while the world is paused. GameMode owns fatal-damage
ordering and hands off to replay before normal final settlement.

No paid requests, art generation or new binary assets are required. Preserve the
player save and Boss memory; all automated runs use the existing test slot and
test Boss memory directory. Existing tests disable automatic story presentation
unless testing story specifically. New integration tests exercise real feature
entry points, including replay start/finish/skip and deadly bullet cleanup.

## Verification

Add regressions for swept collision, immune-then-vulnerable bullet, delayed
bullets, same-frame accepted hit accounting and fatal cleanup; pure health tests
for clamp/heal/hero/charm/death; story chapter/ending consistency tests; bounded
replay history tests plus PIE replay and story checks. Compile with UE 5.8,
run targeted suites then the full suite. Run rendered visual checks for HP,
dialogue and replay and inspect screenshots. Historical 69/69 is a baseline,
not evidence for these changes.
