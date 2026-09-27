# ttfx-haiku roadmap

## Current status

The dependency-free C++17 port builds and runs on 64-bit Haiku. It currently
supports all thirty-seven upstream effects: `beams`, `binarypath`, `blackhole`, `bouncyballs`,
`bubbles`, `burn`, `colorshift`, `crumble`, `decrypt`, `errorcorrect`,
`expand`, `fireworks`, `highlight`, `laseretch`, `matrix`, `middleout`,
`orbittingvolley`, `overflow`, `pour`, `print`, `rain`, `randomsequence`,
`rings`, `scattered`, `slice`, `slide`, `smoke`, `spotlights`, `spray`,
`sweep`, `swarm`, `synthgrid`, `thunderstorm`, `unstable`, `vhstape`, `waves`,
and `wipe`. Terminal repainting, resize restart,
interruption, UTF-8 validation, true-color output,
deterministic seeded randomness, file/stdin input, and platform-separated
builds are implemented.

The existing effects are usable but are not yet frame-for-frame reproductions
of the Rust reference. Visual parity work should happen alongside, rather than
after, new effect ports.

## Effect coverage

All 37 effects from the Rust reference are now present. The remaining work is
visual parity, shared-engine cleanup, CLI compatibility, and Haiku packaging.

## Engine work remaining

1. Separate canvas, gradient, easing, path, scene, and grouping code from the
   growing `effects.cpp` implementation.
2. Add layers and collision rules so several characters can cross the same
   cell predictably.
3. Add reusable movement paths, waypoints, per-character delays, scene frames,
   and completion events.
4. Support all canvas anchors, text anchors, wrapping, tab expansion, explicit
   canvas sizes, and ignored terminal dimensions.
5. Handle ANSI-styled input and the upstream existing-color policies.
6. Add display-width handling for combining and wide Unicode characters. The
   current layout validates UTF-8 but treats every code point as one cell.
7. Match terminal teardown and broken-pipe behavior under SIGINT and SIGTERM.
8. Add effect-specific command-line options and shell completion.

## Compatibility and verification

- Build a deterministic frame-trace harness that runs the Rust reference and
  C++ port with identical input and seed, then reports the first differing
  frame.
- Add golden tests for easing, gradients, grouping, geometry, and RNG output.
- Exercise single-line, multi-line, narrow-terminal, resize, interruption,
  redirected-output, invalid UTF-8, and broken-pipe cases on Haiku.
- Test performance and memory use with large ASCII art.
- Keep warning-as-error, AddressSanitizer, and UndefinedBehaviorSanitizer jobs
  running on the development host.

## Distribution

1. Add application metadata and a HaikuPorts recipe once the CLI and file
   layout stabilize.
2. Test installation under `/boot/system/non-packaged` and package upgrades.
3. Write user-facing effect and option documentation.
4. Submit reusable engine improvements upstream where appropriate.

## Later: native ScreenSaver add-on

The command-line renderer writes ANSI terminal frames. A native Haiku
`ScreenSaver` add-on should be a separate target using the same effect state
and timing engine but drawing into a `BView`. Keeping rendering behind a small
interface is therefore a design requirement for the remaining engine work.
