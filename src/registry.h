#ifndef TTFX_REGISTRY_H
#define TTFX_REGISTRY_H

#include "effects.h"

#include <array>
#include <string_view>

namespace ttfx {

using EffectRunner = bool (*)(const std::vector<Glyph>&, const EffectOptions&,
	TerminalSession&, const volatile sig_atomic_t&, const volatile sig_atomic_t&);

struct EffectDescriptor {
	std::string_view name;
	std::string_view description;
	EffectRunner run;
};

const std::array<EffectDescriptor, 37>& effect_registry();
const EffectDescriptor* find_effect(std::string_view name);

} // namespace ttfx

#endif
