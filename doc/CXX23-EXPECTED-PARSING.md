# C++23 `std::expected`: strict HTTP authentication IDs

In `src/web/auth.cpp` two untrusted form fields (`AID`, `GDID`) used
`a2i()`. That legacy helper deliberately follows `atoi` semantics:
unparseable text becomes zero, trailing text is ignored and overflowing
values saturate. Those semantics are useful for legacy game data imports,
**not** for authorization identifiers.

`src/common/parse.hpp` adds a separate, strict decimal parser using:

- C++23 `std::expected<T, decimal_error>` for explicit success/error;
- `std::from_chars` to parse without exceptions, locale or allocations;
- C++20 concepts to restrict the numeric types;
- complete-input validation and explicit positive-int32 validation for
  SQL-bound account/guild identifiers.

The web authorization path now **fails closed before querying SQL** for
empty, malformed, negative, zero, or overflowing AID/GDID. Valid decimal
IDs (including leading zeros) follow the same SQL lookups as before.
No changes to `a2i`, gameplay, item/monster imports or database schema.

`tools/expected-parse-check.cpp` exercises 18 cases against the same
`positive_id` helper called by the authorization handler, on both GCC14
and Clang19 via `modernization-contracts`.

**Compatibility difference (intentional):** `+42`, ` 42`, `42 ` and
`42garbage` are no longer accepted as authentication IDs. These are
malformed HTTP field values; the prior permissive conversion could map
them to different account identifiers. This PR makes the policy explicit.

**Limit:** the existing integration suite does not yet drive a live HTTP
request against `isAuthorized`, so the static helper regression is not a
substitute for an end-to-end login/token authorization test. Require the
full multi-platform C++23 build and relevant behavior gates before merging.

## Clang compatibility

Clang 18 is missing `__cpp_concepts=202002L`, a required capability
for libstdc++'s implementation of `<expected>`. Upgrading the Clang CI
baseline to **Clang 19** fixes the compiler feature gate natively without
forging standard feature macros or adding a third-party replacement.
The CI uses GCC 14's standard library on Linux. See the official
[Clang 19 release notes](https://releases.llvm.org/19.1.0/tools/clang/docs/ReleaseNotes.html).
