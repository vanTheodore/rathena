# Corpus regression gates (C++17 → C++23 modernization)

The prior rAthena implementation used C++17. Now the repository builds with strict
ISO C++23. A successful compilation alone does not show that the existing game
content remains compatible; this CI gate consumes the **tracked source corpus**.

## Checks

- **NPC manifests:** parse every tracked `npc/scripts_*.conf`, consider only
  active `npc:` directives, verify each referenced script exists, and protect
  against large accidental omissions with a conservative floor of 350 active
  references (376 on the audited 2026-10-29 snapshot). Disabled/commented
  scripts are intentionally **not** enabled by this check.
- **YAML:** parse every tracked `*.yml` in `db/`, `conf/`, and `npc/`
  with a secondary Python YAML parser. Check the documented `Header` shape
  where present, and verify `Footer.Imports` references. A generated
  `db/import/` or `conf/import/` path may resolve to its checked-in
  `import-tmpl/` source. Empty/comment-only templates are allowed.
- **SQL:** use a disposable MariaDB 11.4 service to import every root
  `sql-files/*.sql`, with `main` before dependent files and roulette data
  last. Assert a substantive number of tables was created.

## What these checks do NOT prove

- PyYAML and rAthena's C++ YAML loaders differ; this is a complementary
  syntax/reference check, not a claim of exact YAML runtime parity.
- Reading NPC manifests is not equivalent to executing NPC scripts or their
  commands, triggers, events, combat logic or quest transitions.
- SQL import is not equivalent to transaction/reconnect, mail, inventory,
  character, or login functionality.
- User-provided custom assets, untracked import directories and conditional
  content not present in the checkout are not covered.
- This workflow does not compare a C++17 executable with C++23 executable.
  Differential behavior testing against a pinned C++17 baseline remains
  future work.

## Next runtime layer

Use the same MariaDB ephemeral fixtures to boot real login/char/map servers,
load the native databases and enabled NPC scripts and assert a fully ready
map-server state (rather than just process startup). Then run the existing
`tools/proto-smoke.ps1` with a disposable account and fixture character,
and assert SQL state changes. Keep this separate from quick corpus checks so
failures localize cleanly.

This incremental coverage supports the modernization rule: no intentional
gameplay, protocol, script or persistence changes.
