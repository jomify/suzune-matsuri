# Version control

The standalone GitHub repository `jomify/suzune-matsuri` contains the game at
its root. In the original local `Group project` checkout, the same game is
tracked under `homework/` in a shared parent repository. Do not initialize a
nested repository there.

## Baseline

The baseline tag is `suzune-matsuri-baseline-2026-10-30`. It captures the source,
Unreal project/configuration/content, source art, asset-generation scripts and
task manifests, assignment brief, design documents, work logbook and screenshots.

Binary assets are stored in Git LFS using `homework/.gitattributes`. This includes
Unreal packages, models, images, audio and document/archive files. Keep the LFS
rules in place before adding new binary assets.

Build output, Unreal caches, runtime logs, save games, Boss memories, local API
keys and `.omx/` session state are excluded. Existing local files are preserved;
the baseline does not back up player progress or learned Boss memories.

This is a development baseline, not a release acceptance claim. The existing
full-suite log records 69 passing tests with `-nullrhi`; no new gameplay test is
performed just to establish version control. The later play log contains an
array-mutation ensure in `AMatsuriDanmakuField::Tick` when the player loses the
last life. Packaging, another-machine verification and sustained learning gains
remain unverified. See `logbook.md` for development history.

## Restore a checkout

For the standalone repository, follow the cloning instructions in the
[main README](../README.md). In the original shared checkout, run these
commands from the parent repository root:

```bash
git lfs install --local
git lfs pull --include="homework/**"
```

Open `homework/SuzuneMatsuri/SuzuneMatsuri.uproject` with UE 5.8 and regenerate or
build editor binaries as described in the [main README](../README.md).

For a separate checkout of the baseline, from the repository root:

```bash
git worktree add --detach ../suzune-baseline suzune-matsuri-baseline-2026-10-30
```

## Commit and publish from the shared checkout

From `homework/`, stage and review this project's changes:

```bash
git add -- .
git diff --cached --name-status
git diff --cached --check
git lfs status
```

The repository also contains other projects. Confirm the index contains only the
intended files before committing. Use the workspace's Lore commit format: an
intent line explaining why, useful context, and relevant Git trailers such as
`Tested:` and `Not-tested:`. Never add secrets or include generated caches.

To publish updates derived from the shared checkout, run the following from
the parent repository root. The standalone remote must already be configured
as `suzune-matsuri`.

```bash
git subtree split --prefix=homework -b suzune-matsuri-export
git lfs push --all suzune-matsuri suzune-matsuri-export
git push suzune-matsuri suzune-matsuri-export:main
```

GitHub Releases holds the playable Windows ZIP. Local builds, Fab source
assets, reference audio, saves, and Boss memory stay outside the source export.
