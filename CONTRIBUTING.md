# Contributing to ttfx-haiku

Thanks for helping improve `ttfx-haiku`.

## Before opening a pull request

1. Build from a clean tree with `make clean && make`.
2. Run the complete test suite with `make test`.
3. Test terminal-facing changes on Haiku when possible.
4. Keep changes dependency-free and compatible with C++17.

Use tabs for C++ indentation and follow the style of the surrounding code.
Keep generated binaries, object files, dependency files, and platform build
directories out of commits.

## Effect changes

The Rust `omacom/ttfx` project is the behavioral reference. New effect work
should preserve the final input text, respond to interruption and resize
signals, honor `--no-color` and `--frame-rate`, and produce identical output
for repeated runs using the same `--seed`.

When visual behavior intentionally differs from the reference, describe the
difference in the pull request and update `ROADMAP.md` if it affects parity.

## Reporting bugs

Please include the operating-system version, terminal, compiler, command line,
input text, and whether the problem reproduces with `--no-color` or a fixed
`--seed`.
