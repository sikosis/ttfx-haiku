#include "effects.h"
#include "registry.h"
#include "rng.h"
#include "terminal.h"
#include "version.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

volatile sig_atomic_t gInterrupted = 0;
volatile sig_atomic_t gResized = 0;

void handle_interrupt(int)
{
	gInterrupted = 1;
}

void handle_resize(int)
{
	gResized = 1;
}

struct Options {
	int frame_rate = 60;
	bool color = true;
	bool help = false;
	bool version = false;
	bool list_effects = false;
	bool random_effect = false;
	std::uint64_t seed = 0;
	bool seed_given = false;
	std::string input_file;
	std::string effect;
};

int parse_integer(const std::string& text, const char* option, int minimum)
{
	char* end = nullptr;
	errno = 0;
	const long value = std::strtol(text.c_str(), &end, 10);
	if (errno != 0 || *end != '\0' || value < minimum || value > 100000)
		throw std::runtime_error(std::string("invalid value for ") + option + ": " + text);
	return static_cast<int>(value);
}

Options parse_options(int argc, char** argv)
{
	Options options;
	for (int index = 1; index < argc; ++index) {
		const std::string argument(argv[index]);
		if (argument == "--help" || argument == "-h")
			options.help = true;
		else if (argument == "--version" || argument == "-v")
			options.version = true;
		else if (argument == "--list-effects")
			options.list_effects = true;
		else if (argument == "--no-color")
			options.color = false;
		else if (argument == "--random-effect" || argument == "-R")
			options.random_effect = true;
		else if (argument == "--frame-rate") {
			if (++index == argc)
				throw std::runtime_error("--frame-rate requires a value");
			options.frame_rate = parse_integer(argv[index], "--frame-rate", 0);
		} else if (argument == "--seed") {
			if (++index == argc)
				throw std::runtime_error("--seed requires a value");
			options.seed = static_cast<std::uint64_t>(parse_integer(argv[index], "--seed", 0));
			options.seed_given = true;
		} else if (argument == "--input-file" || argument == "-i") {
			if (++index == argc)
				throw std::runtime_error("--input-file requires a path");
			options.input_file = argv[index];
		} else if (!argument.empty() && argument[0] == '-') {
			throw std::runtime_error("unknown option: " + argument);
		} else if (!options.effect.empty()) {
			throw std::runtime_error("only one effect may be specified");
		} else
			options.effect = argument;
	}
	return options;
}

void print_help()
{
	std::cout
		<< "ttfx v" << ttfx::kVersion << " - Terminal text effects for Haiku\n"
		<< "Dependency-free C++ port of omacom/ttfx\n\n"
		<< "Usage: ttfx [OPTIONS] EFFECT\n\n"
		<< "Effects:\n";
	for (const ttfx::EffectDescriptor& effect : ttfx::effect_registry())
		std::cout << "  " << std::left << std::setw(18) << effect.name << effect.description << '\n';
	std::cout << "\nOptions:\n"
		<< "  -h, --help         Print help\n"
		<< "  -v, --version      Print version\n"
		<< "  -i, --input-file   Read text from a file\n"
		<< "      --frame-rate   Frames per second (default: 60)\n"
		<< "      --no-color     Disable ANSI color\n"
		<< "  -R, --random-effect  Select a supported effect\n"
		<< "      --list-effects  Print supported effect names\n"
		<< "      --seed         Random seed for compatible scripts\n";
}

std::string read_input(const std::string& path)
{
	if (!path.empty()) {
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			throw std::runtime_error("cannot open input file: " + path);
		return std::string(std::istreambuf_iterator<char>(stream), {});
	}
	if (ttfx::stdin_is_terminal())
		return {};
	return std::string(std::istreambuf_iterator<char>(std::cin), {});
}

} // namespace

int main(int argc, char** argv)
{
	try {
		const Options options = parse_options(argc, argv);
		if (options.help) {
			print_help();
			return 0;
		}
		if (options.version) {
			std::cout << "ttfx v" << ttfx::kVersion << '\n';
			return 0;
		}
		if (options.list_effects) {
			for (const ttfx::EffectDescriptor& effect : ttfx::effect_registry())
				std::cout << effect.name << '\n';
			return 0;
		}

		std::string effect = options.effect;
		if (options.random_effect) {
			ttfx::Rng rng = options.seed_given
				? ttfx::Rng(options.seed) : ttfx::Rng::from_entropy();
			const auto& effects = ttfx::effect_registry();
			effect = effects[static_cast<std::size_t>(rng.below(effects.size()))].name;
		}
		if (effect.empty())
			throw std::runtime_error("no effect specified");
		const ttfx::EffectDescriptor* selected_effect = ttfx::find_effect(effect);
		if (selected_effect == nullptr)
			throw std::runtime_error("unknown effect: " + effect);

		const std::string input = read_input(options.input_file);
		if (input.find_first_not_of(" \t\r\n") == std::string::npos) {
			std::cout << "NO INPUT.\n";
			return 1;
		}
		const std::vector<ttfx::Glyph> glyphs = ttfx::layout_utf8(input);

		std::signal(SIGINT, handle_interrupt);
		std::signal(SIGTERM, handle_interrupt);
#ifdef SIGWINCH
		std::signal(SIGWINCH, handle_resize);
#endif

		ttfx::TerminalSession terminal(ttfx::stdout_is_terminal());
		ttfx::EffectOptions effect_options;
		effect_options.frame_rate = options.frame_rate;
		effect_options.color = options.color;
		effect_options.seed = options.seed;
		effect_options.seed_given = options.seed_given;
		for (;;) {
			const bool completed = selected_effect->run(
				glyphs, effect_options, terminal, gInterrupted, gResized);
			if (completed)
				return 0;
			if (gInterrupted != 0)
				return 1;
			if (gResized != 0) {
				gResized = 0;
				continue;
			}
			return 1;
		}
	} catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}
