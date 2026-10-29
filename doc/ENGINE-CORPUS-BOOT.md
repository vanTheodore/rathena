# Full engine boot against the checked-in corpus

`.github/workflows/engine-corpus-runtime.yml` adds a real runtime layer
after the static YAML/NPC manifest and SQL schema validation.

1. Build login-server, char-server and map-server with GCC 14 / ISO C++23,
   using the project's ordinary CMake path and at most two build workers.
2. Start a disposable MariaDB 11.4 container, install `sql-files/main.sql`,
   `logs.sql`, `web.sql` and roulette fixtures, then stage the checked-in
   import templates. The default internal server account `s1` is present
   in `main.sql`. No external credentials are accessed.
3. Start real login → char → map processes in that order. Require each to
   reach its own readiness log while remaining alive.
4. Require `map-server`'s actual source-defined reports: a complete map
   cache (at least 1000 maps), a loaded NPC corpus (at least 100 NPCs),
   followed by the final ready state. This tests the C++23 rAthena parser,
   not a separate YAML parser.
5. Always upload logs for failure diagnosis and terminate all spawned
   servers on exit.

## Boundaries

This is **not** a game-client integration test and does not simulate character
creation, trading, combat, item drops, persistence, packet versions or quests.
The thresholds are conservative; the exact counts are printed and must be
read before declaring a stronger parity contract. The script/NPC set is
the set enabled by the repository's original config, not every disabled or
custom script. The service's hard-coded passwords are ephemeral CI fixtures.

Follow-up: seed a disposable player account and initial character then use
`tools/proto-smoke.ps1` to traverse login → char → map by actual packets and
assert a database save; later add scenarios for mail, SQL reconnect, inventory,
and expected C++17 vs C++23 semantics.

CI gate: require a **completed green** engine-corpus-runtime job before
merging. Unlike a compiler-only smoke, map readiness and load counters are
evidence that the server consumed the bundled game content.
