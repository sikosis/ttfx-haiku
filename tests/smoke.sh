#!/bin/sh
set -eu

binary=${1:-./ttfx}

"$binary" --version | grep -q '^ttfx v0.37$'
"$binary" --help | grep -q '^ttfx v0.37 - Terminal text effects for Haiku$'
"$binary" --help | grep -q '^  expand'
test "$("$binary" --list-effects | wc -l | tr -d ' ')" = 37
"$binary" --list-effects | grep -q '^decrypt$'
"$binary" --list-effects | grep -q '^wipe$'
for effect in beams colorshift highlight waves; do
	"$binary" --list-effects | grep -q "^${effect}$"
done
for effect in binarypath bouncyballs bubbles burn; do
	"$binary" --list-effects | grep -q "^${effect}$"
done
for effect in fireworks laseretch overflow rings; do
	"$binary" --list-effects | grep -q "^${effect}$"
done
for effect in blackhole crumble smoke synthgrid; do
	"$binary" --list-effects | grep -q "^${effect}$"
done
for effect in matrix orbittingvolley spotlights swarm; do
	"$binary" --list-effects | grep -q "^${effect}$"
done
for effect in thunderstorm unstable vhstape; do
	"$binary" --list-effects | grep -q "^${effect}$"
done
printf 'Haiku\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 expand >/dev/null
printf 'Haiku\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 slide >/dev/null
printf 'Haiku\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 wipe >/dev/null
printf 'Haiku\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 pour >/dev/null
printf 'Haiku\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 spray >/dev/null
printf 'Haiku\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 decrypt >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 print >/dev/null
printf 'Haiku\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 randomsequence >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 middleout >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 slice >/dev/null
printf 'Haiku\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 scattered >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 rain >/dev/null
printf 'Hello Haiku\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 errorcorrect >/dev/null
printf 'Hello Haiku\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 sweep >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 beams >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 colorshift >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 highlight >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 waves >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 binarypath >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 bouncyballs >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 bubbles >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 burn >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 fireworks >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 laseretch >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 overflow >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 rings >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 blackhole >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 crumble >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 smoke >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 synthgrid >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 matrix >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --no-color --frame-rate 0 orbittingvolley >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 spotlights >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 swarm >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 thunderstorm >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 unstable >/dev/null
printf 'one\ntwo\n' | COLUMNS=40 LINES=12 "$binary" --seed 7 --no-color --frame-rate 0 vhstape >/dev/null

first=${TMPDIR:-/tmp}/ttfx-seeded-first.$$
second=${TMPDIR:-/tmp}/ttfx-seeded-second.$$
trap 'rm -f "$first" "$second"' EXIT HUP INT TERM
printf 'deterministic\n' | "$binary" --seed 42 --no-color --frame-rate 0 spray >"$first"
printf 'deterministic\n' | "$binary" --seed 42 --no-color --frame-rate 0 spray >"$second"
cmp "$first" "$second"
printf 'deterministic\n' | "$binary" --seed 42 --no-color --frame-rate 0 randomsequence >"$first"
printf 'deterministic\n' | "$binary" --seed 42 --no-color --frame-rate 0 randomsequence >"$second"
cmp "$first" "$second"
for effect in beams binarypath blackhole bouncyballs bubbles burn crumble errorcorrect fireworks laseretch matrix overflow rain rings scattered smoke spotlights swarm synthgrid thunderstorm unstable vhstape; do
	printf 'deterministic\n' | "$binary" --seed 42 --no-color --frame-rate 0 "$effect" >"$first"
	printf 'deterministic\n' | "$binary" --seed 42 --no-color --frame-rate 0 "$effect" >"$second"
	cmp "$first" "$second"
done

if printf '\377\n' | "$binary" --no-color --frame-rate 0 expand >/dev/null 2>&1; then
	echo "invalid UTF-8 was accepted" >&2
	exit 1
fi

if printf '\300\200\n' | "$binary" --no-color --frame-rate 0 expand >/dev/null 2>&1; then
	echo "overlong UTF-8 was accepted" >&2
	exit 1
fi

if printf '\355\240\200\n' | "$binary" --no-color --frame-rate 0 expand >/dev/null 2>&1; then
	echo "UTF-8 surrogate was accepted" >&2
	exit 1
fi

printf 'this text is deliberately wider than the test canvas\n' \
	| COLUMNS=8 LINES=2 "$binary" --no-color --frame-rate 0 expand >/dev/null

echo "smoke tests passed"
