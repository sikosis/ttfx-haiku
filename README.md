# ttfx-haiku

`ttfx-haiku` is a dependency-free C++17 terminal animation engine for Haiku
that recreates [`omacom/ttfx`](https://github.com/omacom/ttfx) text effects
with deterministic, native builds.

The project implements all 37 effects exposed by the Rust reference while
using only the C++ standard library and operating-system APIs. It is designed
for Haiku and also builds on other POSIX-like systems for development and
testing.

## Features

- all 37 upstream effect names, selectable individually or at random
- no third-party runtime dependencies, Cargo, CMake, or generated sources
- native Haiku terminal sizing through `tcgetwinsize()`
- UTF-8 validation and code-point layout
- terminal resize and interruption handling with cursor restoration
- true-color output with a `--no-color` mode
- deterministic xoshiro256++ randomness through `--seed`
- file, pipe, and standard-input support
- platform-separated build directories

Version numbers track effect coverage: each implemented effect contributes
0.01, making the complete effect set `v0.37`.

## Requirements

- a C++17 compiler
- POSIX `make`
- an ANSI-compatible terminal; true-color support is recommended

Haiku is the primary target. macOS and Linux are useful development hosts.

## Build and test

```sh
make
make test
```

Build products are kept under `build/<system>-<architecture>/`; the runnable
`ttfx` binary is copied to the project root.

## Usage

Pipe text into a named effect:

```sh
printf 'Hello, Haiku!\n' | ./ttfx expand
```

Read text from a file:

```sh
./ttfx --input-file banner.txt matrix
```

Choose an effect at random or make a random effect reproducible:

```sh
printf 'Hello!\n' | ./ttfx --random-effect
printf 'Hello!\n' | ./ttfx --random-effect --seed 42
```

Discover available effects and options:

```sh
./ttfx --list-effects
./ttfx --help
```

## Install

The default prefix follows Haiku's non-packaged software convention:

```sh
make install
```

For packaging or staged installation, override `PREFIX` and `DESTDIR`:

```sh
make install PREFIX=/boot/system/non-packaged DESTDIR=/tmp/package-root
```

## Compatibility status

The C++ port provides a usable native implementation of every upstream effect,
but it is not yet a frame-for-frame reproduction. Effect-specific command-line
options, ANSI-styled input policies, wide-character display widths, and some
advanced layering behavior remain future compatibility work. See
[`ROADMAP.md`](ROADMAP.md) for details.

## Contributing

Bug reports and patches are welcome. Read [`CONTRIBUTING.md`](CONTRIBUTING.md)
before submitting a pull request.

## License and attribution

This project is available under the MIT License. See [`LICENSE`](LICENSE).
It is a clean C++ implementation based on the behavior of `omacom/ttfx`, which
itself was ported from ChrisBuilds' TerminalTextEffects.
