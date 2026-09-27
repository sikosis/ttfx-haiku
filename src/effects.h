#ifndef TTFX_EFFECTS_H
#define TTFX_EFFECTS_H

#include "terminal.h"

#include <csignal>
#include <cstdint>
#include <string>
#include <vector>

namespace ttfx {

struct EffectOptions {
	int frame_rate = 60;
	bool color = true;
	std::uint64_t seed = 0;
	bool seed_given = false;
};

bool run_expand(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_wipe(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_slide(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_pour(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_spray(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_decrypt(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_print(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_randomsequence(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_middleout(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_slice(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_scattered(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_rain(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_errorcorrect(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_sweep(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_highlight(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_colorshift(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_waves(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_beams(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_binarypath(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_bouncyballs(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_bubbles(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_burn(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_fireworks(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_laseretch(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_overflow(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_rings(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_smoke(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_synthgrid(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_blackhole(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_crumble(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_matrix(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_orbittingvolley(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_spotlights(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_swarm(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_thunderstorm(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_unstable(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

bool run_vhstape(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized);

} // namespace ttfx

#endif
