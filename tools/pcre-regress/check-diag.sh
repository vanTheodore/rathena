#!/bin/sh
# Assert the PCRE2 engine-status *categories* for known corpus cases.
#
# The pinned parity output (pcre1.expected) deliberately normalizes every
# negative pcre2_match() result into "no match", exactly like the PCRE1 code
# did - that collapse is the behavior rAthena exposes, so it must stay. This
# script checks the engine still distinguishes the categories underneath, as
# documented in doc/MODERNIZATION.md:
#
#   1   true no-match                 -> PCRE2_ERROR_NOMATCH
#   26  start offset past the subject -> PCRE2_ERROR_BADOFFSET
#   27  invalid pattern               -> pcre2_compile() error code
#   33  ordinary match                -> r > 0 (capture count)
#   45  more than 10 capture pairs    -> r == 0 (ovector too small, not an error)
#   47  catastrophic backtracking     -> an execution limit (MATCH/DEPTH/HEAP)
#
# The indices are the corpus line order (0-based, comment/blank lines skipped),
# so they shift when a case is inserted above one of them - update accordingly.
#
# Usage: check-diag.sh <file produced by `pcre-regress --diag <corpus>`>
set -eu

diag="$1"
if [ ! -f "$diag" ]; then
	echo "check-diag: no such diag output: $diag" >&2
	exit 2
fi

# Tolerate CRLF (Windows runs) so the same script works everywhere.
normalized="$(mktemp)"
trap 'rm -f "$normalized"' EXIT
tr -d '\r' < "$diag" > "$normalized"

failed=0
expect() { # <ERE> <description>
	if ! grep -qE "$1" "$normalized"; then
		printf 'check-diag: FAIL: %s (nothing matches /%s/)\n' "$2" "$1" >&2
		failed=1
	fi
}

expect '^1[|]match[|]-1[|]NOMATCH$'         'case 1: a true no-match is NOMATCH'
expect '^26[|]match[|]-[0-9]+[|]BADOFFSET$' 'case 26: offset past the end is BADOFFSET'
expect '^27[|]compile_err[|][0-9]+$'        'case 27: invalid pattern fails at compile time'
expect '^33[|]match[|][1-9][0-9]*[|]OK$'    'case 33: a real match reports its capture count'
expect '^45[|]match[|]0[|]OVEC_SMALL$'      'case 45: >10 capture pairs returns r == 0'
expect '^47[|]match[|]-[0-9]+[|]LIMIT$'     'case 47: catastrophic backtracking hits an execution limit'

exit "$failed"
