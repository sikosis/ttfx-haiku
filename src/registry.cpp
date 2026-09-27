#include "registry.h"

namespace ttfx {

const std::array<EffectDescriptor, 37>& effect_registry()
{
	static const std::array<EffectDescriptor, 37> effects = {{
		{"beams", "Sweep crossing beams over the text, then reveal it", run_beams},
		{"binarypath", "Route binary characters into their final positions", run_binarypath},
		{"blackhole", "Consume the starfield into a black hole, then explode", run_blackhole},
		{"bouncyballs", "Bounce characters down into their final positions", run_bouncyballs},
		{"bubbles", "Float character bubbles upward, pop, and settle", run_bubbles},
		{"burn", "Burn through the text and reveal its final colors", run_burn},
		{"colorshift", "Cycle a travelling rainbow across the text", run_colorshift},
		{"crumble", "Crumble, vacuum, and rebuild the text", run_crumble},
		{"decrypt", "Type ciphertext, then reveal the input", run_decrypt},
		{"errorcorrect", "Correct characters placed in the wrong positions", run_errorcorrect},
		{"expand", "Expand characters outward from the center", run_expand},
		{"fireworks", "Launch and burst characters before they settle", run_fireworks},
		{"highlight", "Pass a bright diagonal highlight over the text", run_highlight},
		{"laseretch", "Etch the text with a moving laser and cooling glow", run_laseretch},
		{"matrix", "Rain digital symbols, fill the text, and resolve it", run_matrix},
		{"middleout", "Expand through the center line, then outward", run_middleout},
		{"orbittingvolley", "Orbit launchers and fire characters into position", run_orbittingvolley},
		{"overflow", "Overflow scrolling rows before restoring the text", run_overflow},
		{"pour", "Pour characters downward into position", run_pour},
		{"print", "Print rows using an animated print head", run_print},
		{"rain", "Rain characters down into their final positions", run_rain},
		{"randomsequence", "Reveal characters in a random sequence", run_randomsequence},
		{"rings", "Arrange characters into spinning and dispersing rings", run_rings},
		{"scattered", "Move scattered characters into position", run_scattered},
		{"slice", "Slide two vertical slices into position", run_slice},
		{"slide", "Slide rows into view from the left", run_slide},
		{"smoke", "Spread smoke across the text and paint its colors", run_smoke},
		{"spotlights", "Search the text with moving beams, then illuminate it", run_spotlights},
		{"spray", "Spray characters from the right edge", run_spray},
		{"sweep", "Sweep across the text twice to reveal and color it", run_sweep},
		{"swarm", "Move character swarms through several gathering areas", run_swarm},
		{"synthgrid", "Generate text through an expanding synthwave grid", run_synthgrid},
		{"thunderstorm", "Darken the text beneath rain and lightning strikes", run_thunderstorm},
		{"unstable", "Rumble, explode, and reassemble unstable text", run_unstable},
		{"vhstape", "Glitch text lines through VHS noise and redraw", run_vhstape},
		{"waves", "Send animated character waves across the text", run_waves},
		{"wipe", "Reveal text diagonally with a color wipe", run_wipe},
	}};
	return effects;
}

const EffectDescriptor* find_effect(std::string_view name)
{
	for (const EffectDescriptor& effect : effect_registry()) {
		if (effect.name == name)
			return &effect;
	}
	return nullptr;
}

} // namespace ttfx
