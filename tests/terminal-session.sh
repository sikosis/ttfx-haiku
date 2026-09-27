#!/bin/sh
set -eu

binary=${1:?terminal session test binary required}
actual=${TMPDIR:-/tmp}/ttfx-terminal-actual.$$
expected=${TMPDIR:-/tmp}/ttfx-terminal-expected.$$
trap 'rm -f "$actual" "$expected"' EXIT HUP INT TERM

"$binary" >"$actual"
printf '\033[?25lfirst\n\033[1Asecond\n\033[1Atop\nbottom\n\033[2Anew top\nnew bottom\n\033[0m\033[?25h' >"$expected"
cmp "$expected" "$actual"

echo "terminal session tests passed"
