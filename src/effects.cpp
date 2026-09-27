#include "effects.h"
#include "rng.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <thread>

namespace ttfx {
namespace {

double in_out_quart(double value)
{
	if (value < 0.5)
		return 8.0 * value * value * value * value;
	const double shifted = -2.0 * value + 2.0;
	return 1.0 - shifted * shifted * shifted * shifted / 2.0;
}

struct Color {
	int red;
	int green;
	int blue;
};

struct CharacterState {
	int column = 0;
	int row = 0;
	bool visible = false;
	Color color = {255, 255, 255};
	std::string symbol;
};

struct CanvasSize {
	int width = 1;
	int height = 1;
};

CanvasSize canvas_size(const std::vector<Glyph>& glyphs)
{
	CanvasSize canvas;
	for (const Glyph& glyph : glyphs) {
		canvas.width = std::max(canvas.width, glyph.column + 1);
		canvas.height = std::max(canvas.height, glyph.row + 1);
	}
	const Size viewport = terminal_size();
	canvas.width = std::min(canvas.width, viewport.columns);
	canvas.height = std::min(canvas.height, viewport.rows);
	return canvas;
}

bool should_stop(const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	return interrupted != 0 || resized != 0;
}

std::chrono::microseconds frame_duration(const EffectOptions& options)
{
	return options.frame_rate > 0
		? std::chrono::microseconds(1000000 / options.frame_rate)
		: std::chrono::microseconds(0);
}

void finish_frame(std::chrono::steady_clock::time_point started,
	std::chrono::microseconds duration)
{
	if (duration.count() == 0)
		return;
	const auto elapsed = std::chrono::steady_clock::now() - started;
	if (elapsed < duration)
		std::this_thread::sleep_for(duration - elapsed);
}

void render(const std::vector<Glyph>& glyphs, const std::vector<CharacterState>& states,
	const CanvasSize& canvas, bool use_color, TerminalSession& terminal)
{
	std::vector<int> cells(static_cast<std::size_t>(canvas.width * canvas.height), -1);
	for (std::size_t index = 0; index < states.size(); ++index) {
		const CharacterState& state = states[index];
		if (!state.visible || state.column < 0 || state.column >= canvas.width
			|| state.row < 0 || state.row >= canvas.height)
			continue;
		cells[static_cast<std::size_t>(state.row * canvas.width + state.column)]
			= static_cast<int>(index);
	}

	std::ostringstream output;
	for (int row = 0; row < canvas.height; ++row) {
		output << "\r\x1b[2K";
		for (int column = 0; column < canvas.width; ++column) {
			const int index = cells[static_cast<std::size_t>(row * canvas.width + column)];
			if (index < 0) {
				output << ' ';
				continue;
			}
			const CharacterState& state = states[static_cast<std::size_t>(index)];
			if (use_color)
				output << rgb(state.color.red, state.color.green, state.color.blue);
			output << (state.symbol.empty()
				? glyphs[static_cast<std::size_t>(index)].text : state.symbol);
			if (use_color)
				output << reset_color();
		}
		output << '\n';
	}
	terminal.draw(output.str(), canvas.height);
}

Color mix(Color first, Color second, double amount)
{
	return {
		static_cast<int>(std::lround(first.red + (second.red - first.red) * amount)),
		static_cast<int>(std::lround(first.green + (second.green - first.green) * amount)),
		static_cast<int>(std::lround(first.blue + (second.blue - first.blue) * amount)),
	};
}

Color gradient(double amount)
{
	const Color purple = {138, 0, 138};
	const Color cyan = {0, 209, 255};
	const Color white = {255, 255, 255};
	if (amount <= 0.5)
		return mix(purple, cyan, amount * 2.0);
	return mix(cyan, white, (amount - 0.5) * 2.0);
}

Color warm_gradient(double amount)
{
	const Color purple = {131, 58, 180};
	const Color red = {253, 29, 29};
	const Color orange = {252, 176, 69};
	if (amount <= 0.5)
		return mix(purple, red, amount * 2.0);
	return mix(red, orange, (amount - 0.5) * 2.0);
}

double in_out_quad(double value)
{
	return value < 0.5 ? 2.0 * value * value
		: 1.0 - std::pow(-2.0 * value + 2.0, 2.0) / 2.0;
}

double in_out_circ(double value)
{
	return value < 0.5
		? (1.0 - std::sqrt(1.0 - std::pow(2.0 * value, 2.0))) / 2.0
		: (std::sqrt(1.0 - std::pow(-2.0 * value + 2.0, 2.0)) + 1.0) / 2.0;
}

double in_quad(double value)
{
	return value * value;
}

double out_expo(double value)
{
	return value >= 1.0 ? 1.0 : 1.0 - std::pow(2.0, -10.0 * value);
}

double in_out_sine(double value)
{
	constexpr double pi = 3.14159265358979323846;
	return -(std::cos(pi * value) - 1.0) / 2.0;
}

double in_out_expo(double value)
{
	if (value <= 0.0)
		return 0.0;
	if (value >= 1.0)
		return 1.0;
	return value < 0.5
		? std::pow(2.0, 20.0 * value - 10.0) / 2.0
		: (2.0 - std::pow(2.0, -20.0 * value + 10.0)) / 2.0;
}

double in_out_back(double value)
{
	constexpr double overshoot = 1.70158;
	constexpr double scaled = overshoot * 1.525;
	if (value < 0.5) {
		const double doubled = 2.0 * value;
		return doubled * doubled * ((scaled + 1.0) * doubled - scaled) / 2.0;
	}
	const double doubled = 2.0 * value - 2.0;
	return (doubled * doubled * ((scaled + 1.0) * doubled + scaled) + 2.0) / 2.0;
}

double out_bounce(double value)
{
	constexpr double scale = 7.5625;
	constexpr double step = 2.75;
	if (value < 1.0 / step)
		return scale * value * value;
	if (value < 2.0 / step) {
		value -= 1.5 / step;
		return scale * value * value + 0.75;
	}
	if (value < 2.5 / step) {
		value -= 2.25 / step;
		return scale * value * value + 0.9375;
	}
	value -= 2.625 / step;
	return scale * value * value + 0.984375;
}

Color scattered_gradient(double amount)
{
	const Color orange = {255, 144, 72};
	const Color lavender = {171, 157, 255};
	const Color mint = {189, 255, 234};
	if (amount <= 0.5)
		return mix(orange, lavender, amount * 2.0);
	return mix(lavender, mint, (amount - 0.5) * 2.0);
}

Color rain_gradient(double amount)
{
	const Color blue = {72, 139, 255};
	const Color pale = {178, 231, 222};
	const Color cyan = {87, 234, 247};
	if (amount <= 0.5)
		return mix(blue, pale, amount * 2.0);
	return mix(pale, cyan, (amount - 0.5) * 2.0);
}

Color rainbow_gradient(double amount)
{
	static const Color stops[] = {
		{232, 20, 22}, {255, 165, 0}, {250, 235, 54}, {121, 195, 20},
		{72, 125, 231}, {75, 54, 157}, {112, 54, 157}, {232, 20, 22}
	};
	const double scaled = std::clamp(amount, 0.0, 1.0) * 7.0;
	const int first = std::min(6, static_cast<int>(scaled));
	return mix(stops[first], stops[first + 1], scaled - first);
}

Color brighten(Color color, double factor)
{
	return {
		std::min(255, static_cast<int>(std::lround(color.red * factor))),
		std::min(255, static_cast<int>(std::lround(color.green * factor))),
		std::min(255, static_cast<int>(std::lround(color.blue * factor))),
	};
}

Rng effect_rng(const EffectOptions& options)
{
	return options.seed_given ? Rng(options.seed) : Rng::from_entropy();
}

} // namespace

bool run_expand(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;

	const CanvasSize canvas = canvas_size(glyphs);
	const int width = canvas.width;
	const int height = canvas.height;
	const double center_column = (width - 1) / 2.0;
	const double center_row = (height - 1) / 2.0;
	const int frame_count = std::max(18, std::min(90, std::max(width, height) * 3));
	const auto frame_time = options.frame_rate > 0
		? std::chrono::microseconds(1000000 / options.frame_rate)
		: std::chrono::microseconds(0);

	for (int frame = 0; frame <= frame_count; ++frame) {
		if (interrupted != 0 || resized != 0)
			return false;
		const auto started = std::chrono::steady_clock::now();
		const double progress = in_out_quart(static_cast<double>(frame) / frame_count);
		std::vector<int> cells(static_cast<std::size_t>(width * height), -1);
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			const int column = static_cast<int>(std::lround(
				center_column + (glyph.column - center_column) * progress));
			const int row = static_cast<int>(std::lround(
				center_row + (glyph.row - center_row) * progress));
			if (column < 0 || column >= width || row < 0 || row >= height)
				continue;
			cells[static_cast<std::size_t>(row * width + column)] = static_cast<int>(index);
		}

		std::ostringstream output;
		for (int row = 0; row < height; ++row) {
			output << "\r\x1b[2K";
			for (int column = 0; column < width; ++column) {
				const int index = cells[static_cast<std::size_t>(row * width + column)];
				if (index < 0) {
					output << ' ';
					continue;
				}
				if (options.color) {
					const double position = width > 1
						? static_cast<double>(glyphs[index].column) / (width - 1)
						: 0.0;
					const Color color = gradient(position);
					output << rgb(color.red, color.green, color.blue);
				}
				output << glyphs[index].text;
				if (options.color)
					output << reset_color();
			}
			output << '\n';
		}
		terminal.draw(output.str(), height);

		if (frame_time.count() > 0) {
			const auto elapsed = std::chrono::steady_clock::now() - started;
			if (elapsed < frame_time)
				std::this_thread::sleep_for(frame_time - elapsed);
		}
	}
	return true;
}

bool run_wipe(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const int last_group = std::max(1, canvas.width + canvas.height - 2);
	const int frame_count = 60;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());

	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const double progress = in_out_circ(static_cast<double>(frame) / frame_count);
		const double threshold = progress * last_group;
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = glyph.column + glyph.row <= threshold;
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			const Color destination = warm_gradient(position);
			const double local = std::clamp(threshold - (glyph.column + glyph.row), 0.0, 1.0);
			state.color = mix({131, 58, 180}, destination, local);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_slide(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const int movement_frames = 28;
	const int row_gap = 2;
	const int frame_count = movement_frames + (canvas.height - 1) * row_gap;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());

	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local_frame = frame - glyph.row * row_gap;
			const double linear = std::clamp(
				static_cast<double>(local_frame) / movement_frames, 0.0, 1.0);
			const double progress = in_out_quad(linear);
			state.column = static_cast<int>(std::lround(
				glyph.column - canvas.width * (1.0 - progress)));
			state.row = glyph.row;
			state.visible = local_frame >= 0;
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix({131, 58, 180}, warm_gradient(position), progress);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_pour(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<int> activation(glyphs.size());
	std::vector<int> travel(glyphs.size());
	int last_activation = 0;
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		const Glyph& glyph = glyphs[index];
		const int group = canvas.height - 1 - glyph.row;
		const int order = group % 2 == 0 ? glyph.column : canvas.width - 1 - glyph.column;
		activation[index] = group * ((canvas.width + 1) / 2 + 1) + order / 2;
		travel[index] = rng.integer(18, 30);
		last_activation = std::max(last_activation, activation[index]);
	}
	const int frame_count = last_activation + 30;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local_frame = frame - activation[index];
			const double linear = std::clamp(
				static_cast<double>(local_frame) / travel[index], 0.0, 1.0);
			const double progress = in_quad(linear);
			state.column = glyph.column;
			state.row = static_cast<int>(std::lround(glyph.row * progress));
			state.visible = local_frame >= 0;
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix({255, 255, 255}, gradient(position), progress);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_spray(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<std::size_t> order(glyphs.size());
	for (std::size_t index = 0; index < order.size(); ++index)
		order[index] = index;
	rng.shuffle(order);
	std::vector<int> activation(glyphs.size());
	std::vector<int> travel(glyphs.size());
	const int volume = std::max(1, static_cast<int>(glyphs.size() * 0.005));
	for (std::size_t sequence = 0; sequence < order.size(); ++sequence) {
		activation[order[sequence]] = static_cast<int>(sequence) / volume;
		travel[order[sequence]] = rng.integer(16, 38);
	}
	const int frame_count = static_cast<int>(glyphs.size()) / volume + 38;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	const double origin_column = canvas.width - 1;
	const double origin_row = (canvas.height - 1) / 2.0;
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local_frame = frame - activation[index];
			const double linear = std::clamp(
				static_cast<double>(local_frame) / travel[index], 0.0, 1.0);
			const double progress = out_expo(linear);
			state.column = static_cast<int>(std::lround(
				origin_column + (glyph.column - origin_column) * progress));
			state.row = static_cast<int>(std::lround(
				origin_row + (glyph.row - origin_row) * progress));
			state.visible = local_frame >= 0;
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix(warm_gradient(rng.random()), gradient(position), progress);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_decrypt(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::vector<std::string> cipher = {
		"#", "%", "&", "*", "+", "0", "1", "?", "@", "X",
		"▉", "▓", "▒", "░", "╬", "╳", "┼", "┤", "└", "┐"
	};
	const int typing_frames = static_cast<int>((glyphs.size() + 1) / 2);
	const int decrypt_frames = 70;
	const int frame_count = typing_frames + decrypt_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = static_cast<int>(index / 2) <= frame;
			if (!state.visible)
				continue;
			const int local = frame - typing_frames;
			const int settle_frame = static_cast<int>(index * 47 % 53);
			if (local < 0 || local < settle_frame) {
				state.symbol = cipher[static_cast<std::size_t>(rng.below(cipher.size()))];
				state.color = {0, rng.integer(128, 255), 0};
			} else {
				state.symbol.clear();
				const double progress = std::clamp(
					static_cast<double>(local - settle_frame) / 12.0, 0.0, 1.0);
				state.color = mix({255, 255, 255}, {237, 160, 0}, progress);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_print(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	std::vector<int> row_start(static_cast<std::size_t>(canvas.height), 0);
	std::vector<int> row_position(static_cast<std::size_t>(canvas.height), 0);
	int timeline = 0;
	for (int row = 0; row < canvas.height; ++row) {
		row_start[static_cast<std::size_t>(row)] = timeline;
		int count = 0;
		for (const Glyph& glyph : glyphs) {
			if (glyph.row == row && glyph.column < canvas.width)
				++count;
		}
		timeline += (count + 1) / 2 + (row + 1 < canvas.height ? 6 : 0);
	}
	const int frame_count = timeline + 6;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		std::fill(row_position.begin(), row_position.end(), 0);
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			if (glyph.row >= canvas.height || glyph.column >= canvas.width)
				continue;
			const int position = row_position[static_cast<std::size_t>(glyph.row)]++;
			const int activation = row_start[static_cast<std::size_t>(glyph.row)] + position / 2;
			const int local = frame - activation;
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = local >= 0;
			if (local == 0)
				state.symbol = "█";
			else if (local == 1)
				state.symbol = "▓";
			else if (local == 2)
				state.symbol = "▒";
			else if (local == 3)
				state.symbol = "░";
			else
				state.symbol.clear();
			const double color_progress = std::clamp(local / 5.0, 0.0, 1.0);
			const double position_color = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix({255, 255, 255},
				mix({2, 184, 189}, {0, 255, 160}, position_color), color_progress);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_randomsequence(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<std::size_t> order(glyphs.size());
	for (std::size_t index = 0; index < order.size(); ++index)
		order[index] = index;
	rng.shuffle(order);
	const int per_frame = std::max(1, static_cast<int>(glyphs.size() * 0.007));
	std::vector<int> activation(glyphs.size());
	for (std::size_t sequence = 0; sequence < order.size(); ++sequence)
		activation[order[sequence]] = static_cast<int>(sequence) / per_frame;
	const int frame_count = static_cast<int>(glyphs.size()) / per_frame + 12;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = local >= 0;
			const double progress = std::clamp(local / 10.0, 0.0, 1.0);
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix({0, 0, 0}, gradient(position), progress);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_middleout(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const int phase_frames = 28;
	const int frame_count = phase_frames * 2;
	const double center_column = (canvas.width - 1) / 2.0;
	const double center_row = (canvas.height - 1) / 2.0;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			double column = center_column;
			double row = center_row;
			double color_progress = 0.0;
			if (frame <= phase_frames) {
				const double progress = in_out_sine(static_cast<double>(frame) / phase_frames);
				column += (glyph.column - center_column) * progress;
			} else {
				const double progress = in_out_sine(
					static_cast<double>(frame - phase_frames) / phase_frames);
				column = glyph.column;
				row += (glyph.row - center_row) * progress;
				color_progress = progress;
			}
			state.column = static_cast<int>(std::lround(column));
			state.row = static_cast<int>(std::lround(row));
			state.visible = true;
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix({255, 255, 255}, gradient(position), color_progress);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_slice(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const int frame_count = 42;
	const double center_column = (canvas.width - 1) / 2.0;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const double progress = in_out_expo(static_cast<double>(frame) / frame_count);
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const double origin = glyph.column <= center_column
				? -static_cast<double>(canvas.height) : canvas.height * 2.0;
			state.column = glyph.column;
			state.row = static_cast<int>(std::lround(origin + (glyph.row - origin) * progress));
			state.visible = true;
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = gradient(position);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_scattered(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<int> start_columns(glyphs.size());
	std::vector<int> start_rows(glyphs.size());
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		start_columns[index] = rng.integer(0, canvas.width - 1);
		start_rows[index] = rng.integer(0, canvas.height - 1);
	}
	const int hold_frames = 25;
	const int movement_frames = 45;
	const int frame_count = hold_frames + movement_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const double linear = std::clamp(
			static_cast<double>(frame - hold_frames) / movement_frames, 0.0, 1.0);
		const double progress = in_out_back(linear);
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = static_cast<int>(std::lround(start_columns[index]
				+ (glyph.column - start_columns[index]) * progress));
			state.row = static_cast<int>(std::lround(start_rows[index]
				+ (glyph.row - start_rows[index]) * progress));
			state.visible = true;
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix({255, 144, 72}, scattered_gradient(position), linear);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_rain(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::vector<std::string> symbols = {"o", ".", ",", "*", "|"};
	const std::vector<Color> colors = {
		{0, 49, 92}, {0, 76, 143}, {0, 117, 219}, {63, 145, 217},
		{120, 185, 242}, {154, 200, 245}, {184, 216, 248}, {227, 239, 252}
	};
	std::vector<int> activation(glyphs.size());
	std::vector<int> travel(glyphs.size());
	std::vector<std::string> rain_symbol(glyphs.size());
	std::vector<Color> rain_color(glyphs.size());
	int last_activation = 0;
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		activation[index] = glyphs[index].row * 3 + rng.integer(0, 8);
		travel[index] = rng.integer(18, 34);
		rain_symbol[index] = symbols[static_cast<std::size_t>(rng.below(symbols.size()))];
		rain_color[index] = colors[static_cast<std::size_t>(rng.below(colors.size()))];
		last_activation = std::max(last_activation, activation[index]);
	}
	const int frame_count = last_activation + 45;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			const double linear = std::clamp(
				static_cast<double>(local) / travel[index], 0.0, 1.0);
			const double progress = linear * linear * linear * linear;
			state.column = glyph.column;
			state.row = static_cast<int>(std::lround(-1.0 + (glyph.row + 1.0) * progress));
			state.visible = local >= 0;
			const double fade = std::clamp((local - travel[index]) / 12.0, 0.0, 1.0);
			state.symbol = fade < 1.0 ? rain_symbol[index] : std::string();
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix(rain_color[index], rain_gradient(position), fade);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_errorcorrect(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<std::size_t> order(glyphs.size());
	for (std::size_t index = 0; index < order.size(); ++index)
		order[index] = index;
	rng.shuffle(order);
	std::vector<std::size_t> partner(glyphs.size());
	for (std::size_t index = 0; index < partner.size(); ++index)
		partner[index] = index;
	const std::size_t pair_count = glyphs.size() >= 2
		? std::max<std::size_t>(1, static_cast<std::size_t>(glyphs.size() * 0.1) / 2) : 0;
	for (std::size_t pair = 0; pair < pair_count; ++pair) {
		const std::size_t first = order[pair * 2];
		const std::size_t second = order[pair * 2 + 1];
		partner[first] = second;
		partner[second] = first;
	}
	const int hold_frames = 32;
	const int movement_frames = 30;
	const int settle_frames = 12;
	const int frame_count = hold_frames + movement_frames + settle_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const double linear = std::clamp(
			static_cast<double>(frame - hold_frames) / movement_frames, 0.0, 1.0);
		const double progress = in_out_quad(linear);
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			const Glyph& origin = glyphs[partner[index]];
			CharacterState& state = states[index];
			state.column = static_cast<int>(std::lround(
				origin.column + (glyph.column - origin.column) * progress));
			state.row = static_cast<int>(std::lround(
				origin.row + (glyph.row - origin.row) * progress));
			state.visible = true;
			const bool erroneous = partner[index] != index;
			if (erroneous && frame < hold_frames && frame % 8 < 3)
				state.symbol = "▓";
			else
				state.symbol.clear();
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			if (erroneous && frame < hold_frames)
				state.color = {255, 0, 0};
			else if (erroneous && linear < 1.0)
				state.color = mix({255, 0, 0}, {0, 255, 0}, progress);
			else {
				const double settle = std::clamp(
					static_cast<double>(frame - hold_frames - movement_frames) / settle_frames,
					0.0, 1.0);
				state.color = mix(erroneous ? Color{0, 255, 0} : gradient(position),
					gradient(position), settle);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_sweep(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const std::vector<std::string> blocks = {"█", "▓", "▒", "░"};
	const int phase_frames = 55;
	const int frame_count = phase_frames * 2;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const bool first_phase = frame <= phase_frames;
		const double linear = first_phase
			? static_cast<double>(frame) / phase_frames
			: static_cast<double>(frame - phase_frames) / phase_frames;
		const double progress = in_out_circ(std::clamp(linear, 0.0, 1.0));
		const double threshold = first_phase
			? (canvas.width - 1) * (1.0 - progress)
			: (canvas.width - 1) * progress;
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = first_phase ? glyph.column >= threshold : true;
			const bool at_edge = std::abs(glyph.column - threshold) < 1.5;
			state.symbol = at_edge
				? blocks[static_cast<std::size_t>(frame / 2) % blocks.size()] : std::string();
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			if (first_phase)
				state.color = {128, 128, 128};
			else
				state.color = glyph.column <= threshold ? gradient(position) : Color{128, 128, 128};
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_highlight(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const int highlight_width = 8;
	const int last_group = std::max(1, canvas.width + canvas.height - 2);
	const int frame_count = 100;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const double progress = in_out_circ(static_cast<double>(frame) / frame_count);
		const double center = -highlight_width + (last_group + highlight_width * 2) * progress;
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = true;
			const double vertical = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			const Color base = gradient(vertical);
			const double group = glyph.column + (canvas.height - 1 - glyph.row);
			const double distance = std::abs(group - center);
			const double glow = std::clamp(1.0 - distance / highlight_width, 0.0, 1.0);
			state.color = mix(base, brighten(base, 1.75), glow);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_colorshift(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const int cycle_frames = 48;
	const int cycles = 3;
	const int settle_frames = 12;
	const int frame_count = cycle_frames * cycles + settle_frames;
	const double center_column = (canvas.width - 1) / 2.0;
	const double center_row = (canvas.height - 1) / 2.0;
	const double max_distance = std::max(1.0, std::hypot(center_column, center_row));
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const bool settling = frame > cycle_frames * cycles;
		const double phase = settling ? 0.0
			: static_cast<double>(frame % cycle_frames) / cycle_frames;
		const double settle = settling
			? static_cast<double>(frame - cycle_frames * cycles) / settle_frames : 0.0;
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = true;
			const double distance = std::hypot(glyph.column - center_column,
				glyph.row - center_row) / max_distance;
			double shifted = phase + distance;
			shifted -= std::floor(shifted);
			const Color travelling = rainbow_gradient(shifted);
			const double position = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			state.color = settling
				? mix(travelling, rainbow_gradient(position), settle) : travelling;
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_waves(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const std::vector<std::string> symbols = {
		"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█", "▇", "▆", "▅", "▄", "▃", "▂", "▁"
	};
	const int wave_count = 3;
	const int symbol_frames = 2;
	const int wave_frames = static_cast<int>(symbols.size()) * symbol_frames * wave_count;
	const int frame_count = canvas.width + wave_frames + 10;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - glyph.column;
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = local >= 0;
			if (local >= 0 && local < wave_frames) {
				const std::size_t symbol = static_cast<std::size_t>(local / symbol_frames)
					% symbols.size();
				state.symbol = symbols[symbol];
				const double color_position = static_cast<double>(symbol)
					/ (symbols.size() - 1);
				state.color = mix({240, 255, 101}, {49, 160, 212},
					1.0 - std::abs(color_position * 2.0 - 1.0));
			} else {
				state.symbol.clear();
				const double diagonal = canvas.width + canvas.height > 2
					? static_cast<double>(glyph.column + glyph.row)
						/ (canvas.width + canvas.height - 2) : 0.0;
				state.color = mix({255, 177, 2}, {49, 160, 212}, diagonal);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_beams(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<unsigned char> reverse_rows(static_cast<std::size_t>(canvas.height));
	std::vector<unsigned char> reverse_columns(static_cast<std::size_t>(canvas.width));
	for (unsigned char& reverse : reverse_rows)
		reverse = static_cast<unsigned char>(rng.below(2));
	for (unsigned char& reverse : reverse_columns)
		reverse = static_cast<unsigned char>(rng.below(2));
	std::vector<int> activation(glyphs.size());
	int last_activation = 0;
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		const Glyph& glyph = glyphs[index];
		const int row_step = reverse_rows[static_cast<std::size_t>(glyph.row)]
			? canvas.width - 1 - glyph.column : glyph.column;
		const int column_step = reverse_columns[static_cast<std::size_t>(glyph.column)]
			? canvas.height - 1 - glyph.row : glyph.row;
		activation[index] = std::min(glyph.row * 4 + row_step * 2,
			glyph.column * 3 + column_step * 3);
		last_activation = std::max(last_activation, activation[index]);
	}
	const int beam_tail = 14;
	const int wipe_frames = 42;
	const int beam_end = last_activation + beam_tail;
	const int frame_count = beam_end + wipe_frames;
	const auto duration = frame_duration(options);
	const std::vector<std::string> row_symbols = {"▂", "▁", "_"};
	const std::vector<std::string> column_symbols = {"▌", "▍", "▎", "▏"};
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const bool wiping = frame > beam_end;
		const double wipe = wiping ? in_out_quad(
			static_cast<double>(frame - beam_end) / wipe_frames) : 0.0;
		const double threshold = wipe * (canvas.width + canvas.height - 2);
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = local >= 0;
			if (!wiping && local >= 0 && local < beam_tail) {
				const bool row_beam = (glyph.row + glyph.column) % 2 == 0;
				const std::vector<std::string>& symbols = row_beam ? row_symbols : column_symbols;
				state.symbol = symbols[static_cast<std::size_t>(local / 2) % symbols.size()];
				state.color = mix({255, 255, 255}, {0, 209, 255},
					static_cast<double>(local) / beam_tail);
			} else {
				state.symbol.clear();
				const double position = canvas.width > 1
					? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
				const Color final_color = gradient(position);
				state.color = wiping && glyph.column + glyph.row <= threshold
					? final_color : mix({0, 0, 0}, final_color, 0.3);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_binarypath(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<int> start_columns(glyphs.size());
	std::vector<int> start_rows(glyphs.size());
	std::vector<int> activation(glyphs.size());
	std::vector<unsigned char> horizontal_first(glyphs.size());
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		if (rng.below(2) == 0) {
			start_columns[index] = rng.below(2) == 0 ? 0 : canvas.width - 1;
			start_rows[index] = rng.integer(0, canvas.height - 1);
		} else {
			start_columns[index] = rng.integer(0, canvas.width - 1);
			start_rows[index] = rng.below(2) == 0 ? 0 : canvas.height - 1;
		}
		activation[index] = static_cast<int>(index % 12) * 2 + rng.integer(0, 5);
		horizontal_first[index] = static_cast<unsigned char>(rng.below(2));
	}
	const int travel_frames = 48;
	const int wipe_frames = 32;
	const int travel_end = 75;
	const int frame_count = travel_end + wipe_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const bool wiping = frame > travel_end;
		const double wipe = wiping ? in_out_quad(
			static_cast<double>(frame - travel_end) / wipe_frames) : 0.0;
		const double threshold = wipe * (canvas.width + canvas.height - 2);
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			const double progress = std::clamp(
				static_cast<double>(local) / travel_frames, 0.0, 1.0);
			if (horizontal_first[index] != 0) {
				if (progress < 0.5) {
					state.column = static_cast<int>(std::lround(start_columns[index]
						+ (glyph.column - start_columns[index]) * progress * 2.0));
					state.row = start_rows[index];
				} else {
					state.column = glyph.column;
					state.row = static_cast<int>(std::lround(start_rows[index]
						+ (glyph.row - start_rows[index]) * (progress - 0.5) * 2.0));
				}
			} else {
				if (progress < 0.5) {
					state.column = start_columns[index];
					state.row = static_cast<int>(std::lround(start_rows[index]
						+ (glyph.row - start_rows[index]) * progress * 2.0));
				} else {
					state.column = static_cast<int>(std::lround(start_columns[index]
						+ (glyph.column - start_columns[index]) * (progress - 0.5) * 2.0));
					state.row = glyph.row;
				}
			}
			state.visible = local >= 0;
			state.symbol = progress < 1.0 ? ((local / 3) % 2 == 0 ? "0" : "1")
				: std::string();
			const double radial = std::hypot(glyph.column - (canvas.width - 1) / 2.0,
				glyph.row - (canvas.height - 1) / 2.0);
			const double maximum = std::max(1.0, std::hypot((canvas.width - 1) / 2.0,
				(canvas.height - 1) / 2.0));
			const Color final_color = mix({0, 213, 0}, {0, 117, 0}, radial / maximum);
			if (progress < 1.0)
				state.color = mix({4, 78, 41}, {149, 237, 135}, progress);
			else
				state.color = wiping && glyph.column + (canvas.height - 1 - glyph.row) <= threshold
					? final_color : mix({0, 0, 0}, final_color, 0.5);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_bouncyballs(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::vector<std::string> symbols = {"*", "o", "O", "0", "."};
	const std::vector<Color> colors = {{209, 244, 165}, {150, 226, 164}, {90, 205, 169}};
	std::vector<std::size_t> order(glyphs.size());
	std::vector<int> activation(glyphs.size());
	std::vector<std::string> ball_symbol(glyphs.size());
	std::vector<Color> ball_color(glyphs.size());
	for (std::size_t index = 0; index < glyphs.size(); ++index)
		order[index] = index;
	rng.shuffle(order);
	for (std::size_t sequence = 0; sequence < order.size(); ++sequence)
		activation[order[sequence]] = static_cast<int>(sequence) * 2;
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		ball_symbol[index] = symbols[static_cast<std::size_t>(rng.below(symbols.size()))];
		ball_color[index] = colors[static_cast<std::size_t>(rng.below(colors.size()))];
	}
	const int movement_frames = 42;
	const int frame_count = static_cast<int>(glyphs.size()) * 2 + movement_frames + 8;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			const double linear = std::clamp(
				static_cast<double>(local) / movement_frames, 0.0, 1.0);
			state.column = glyph.column;
			state.row = static_cast<int>(std::lround(-1.0
				+ (glyph.row + 1.0) * out_bounce(linear)));
			state.visible = local >= 0;
			state.symbol = linear < 1.0 ? ball_symbol[index] : std::string();
			const double diagonal = canvas.width + canvas.height > 2
				? static_cast<double>(glyph.column + glyph.row)
					/ (canvas.width + canvas.height - 2) : 0.0;
			state.color = linear < 1.0 ? ball_color[index]
				: mix({248, 255, 174}, {67, 198, 172}, diagonal);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_bubbles(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::size_t group_size = 6;
	const std::size_t group_count = (glyphs.size() + group_size - 1) / group_size;
	std::vector<int> center_columns(group_count);
	std::vector<int> center_rows(group_count);
	std::vector<double> angle(glyphs.size());
	std::vector<int> radius(glyphs.size());
	for (std::size_t group = 0; group < group_count; ++group) {
		center_columns[group] = rng.integer(0, canvas.width - 1);
		center_rows[group] = rng.integer(0, std::max(0, canvas.height - 1));
	}
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		angle[index] = rng.uniform(0.0, 6.28318530717958647692);
		radius[index] = rng.integer(1, 3);
	}
	const int rise_frames = 38;
	const int pop_frames = 14;
	const int settle_frames = 34;
	const int frame_count = rise_frames + pop_frames + settle_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const std::size_t group = index / group_size;
			const double offset_x = std::cos(angle[index]) * radius[index];
			const double offset_y = std::sin(angle[index]) * radius[index];
			double column;
			double row;
			if (frame <= rise_frames) {
				const double progress = in_out_sine(static_cast<double>(frame) / rise_frames);
				const double wobble = std::sin(frame * 0.35 + group) * 1.5;
				column = center_columns[group] + offset_x * 0.35 + wobble;
				row = canvas.height + 2.0
					+ (center_rows[group] - canvas.height - 2.0) * progress;
				state.symbol = (frame / 5 + static_cast<int>(index)) % 2 == 0 ? "O" : "o";
			} else if (frame <= rise_frames + pop_frames) {
				const double progress = static_cast<double>(frame - rise_frames) / pop_frames;
				column = center_columns[group] + offset_x * progress;
				row = center_rows[group] + offset_y * progress;
				state.symbol = progress < 0.65 ? "*" : ".";
			} else {
				const double linear = static_cast<double>(frame - rise_frames - pop_frames)
					/ settle_frames;
				const double progress = in_out_back(std::clamp(linear, 0.0, 1.0));
				const double popped_column = center_columns[group] + offset_x;
				const double popped_row = center_rows[group] + offset_y;
				column = popped_column + (glyph.column - popped_column) * progress;
				row = popped_row + (glyph.row - popped_row) * progress;
				state.symbol.clear();
			}
			state.column = static_cast<int>(std::lround(column));
			state.row = static_cast<int>(std::lround(row));
			state.visible = true;
			const double position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			state.color = mix({77, 150, 255}, {255, 125, 229}, position);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_burn(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::vector<std::string> burn_symbols = {"'", ".", "▖", "▙", "█", "▜", "▀", "▝", "."};
	const std::vector<Color> burn_colors = {
		{255, 255, 255}, {255, 247, 93}, {254, 101, 13}, {138, 0, 60}, {81, 1, 0}
	};
	const std::vector<std::string> smoke_symbols = {".", ",", "'", "`", "#", "*"};
	const int origin_column = rng.integer(0, canvas.width - 1);
	const int origin_row = rng.integer(0, canvas.height - 1);
	std::vector<int> activation(glyphs.size());
	std::vector<unsigned char> smoke(glyphs.size());
	std::vector<std::string> smoke_symbol(glyphs.size());
	int last_activation = 0;
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		activation[index] = (std::abs(glyphs[index].column - origin_column)
			+ std::abs(glyphs[index].row - origin_row)) * 3 + rng.integer(0, 5);
		smoke[index] = static_cast<unsigned char>(rng.random() < 0.5);
		smoke_symbol[index] = smoke_symbols[static_cast<std::size_t>(rng.below(smoke_symbols.size()))];
		last_activation = std::max(last_activation, activation[index]);
	}
	const int burn_frames = 36;
	const int smoke_frames = 12;
	const int settle_frames = 24;
	const int frame_count = last_activation + burn_frames + smoke_frames + settle_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = true;
			if (local < 0) {
				state.symbol.clear();
				state.color = {131, 115, 115};
			} else if (local < burn_frames) {
				const double progress = static_cast<double>(local) / burn_frames;
				const std::size_t symbol = std::min(burn_symbols.size() - 1,
					static_cast<std::size_t>(progress * burn_symbols.size()));
				const double color_position = progress * (burn_colors.size() - 1);
				const std::size_t color = std::min(burn_colors.size() - 2,
					static_cast<std::size_t>(color_position));
				state.symbol = burn_symbols[symbol];
				state.color = mix(burn_colors[color], burn_colors[color + 1],
					color_position - color);
			} else if (smoke[index] != 0 && local < burn_frames + smoke_frames) {
				const double progress = static_cast<double>(local - burn_frames) / smoke_frames;
				state.row = glyph.row - static_cast<int>(std::lround(progress * 3.0));
				state.column = glyph.column + static_cast<int>(std::lround(
					std::sin(progress * 6.28318530717958647692)));
				state.symbol = smoke_symbol[index];
				state.color = mix({80, 79, 79}, {199, 199, 199}, progress);
			} else {
				state.symbol.clear();
				const double vertical = canvas.height > 1
					? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
				const Color final_color = mix({0, 195, 255}, {255, 255, 28}, vertical);
				const double settle = std::clamp(static_cast<double>(local - burn_frames
					- (smoke[index] != 0 ? smoke_frames : 0)) / settle_frames, 0.0, 1.0);
				state.color = mix({81, 1, 0}, final_color, settle);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_fireworks(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::vector<Color> shell_colors = {
		{136, 247, 226}, {68, 212, 146}, {245, 235, 103}, {255, 161, 92}, {250, 35, 62}
	};
	const std::size_t shell_size = std::max<std::size_t>(2, glyphs.size() / 20);
	const std::size_t shell_count = (glyphs.size() + shell_size - 1) / shell_size;
	std::vector<int> origin_columns(shell_count);
	std::vector<int> apex_rows(shell_count);
	std::vector<Color> colors(shell_count);
	std::vector<double> angles(glyphs.size());
	std::vector<double> distances(glyphs.size());
	for (std::size_t shell = 0; shell < shell_count; ++shell) {
		origin_columns[shell] = rng.integer(0, canvas.width - 1);
		apex_rows[shell] = rng.integer(0, std::max(0, canvas.height - 1));
		colors[shell] = shell_colors[static_cast<std::size_t>(rng.below(shell_colors.size()))];
	}
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		angles[index] = rng.uniform(0.0, 6.28318530717958647692);
		distances[index] = rng.uniform(1.0, std::max(2.0, canvas.width * 0.2));
	}
	const int launch_frames = 20;
	const int burst_frames = 18;
	const int settle_frames = 32;
	const int launch_gap = 12;
	const int frame_count = static_cast<int>(shell_count - 1) * launch_gap
		+ launch_frames + burst_frames + settle_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const std::size_t shell = index / shell_size;
			const int local = frame - static_cast<int>(shell) * launch_gap;
			const double apex_column = origin_columns[shell];
			const double apex_row = apex_rows[shell];
			double column = apex_column;
			double row = canvas.height;
			if (local < launch_frames) {
				const double progress = out_expo(std::clamp(
					static_cast<double>(local) / launch_frames, 0.0, 1.0));
				row += (apex_row - canvas.height) * progress;
				state.symbol = "o";
			} else if (local < launch_frames + burst_frames) {
				const double progress = in_out_circ(static_cast<double>(local - launch_frames)
					/ burst_frames);
				column = apex_column + std::cos(angles[index]) * distances[index] * progress;
				row = apex_row + std::sin(angles[index]) * distances[index] * 0.55 * progress;
				state.symbol = progress < 0.4 ? "*" : glyph.text;
			} else {
				const double linear = std::clamp(static_cast<double>(local - launch_frames
					- burst_frames) / settle_frames, 0.0, 1.0);
				const double progress = in_out_quart(linear);
				const double burst_column = apex_column + std::cos(angles[index]) * distances[index];
				const double burst_row = apex_row + std::sin(angles[index]) * distances[index] * 0.55;
				column = burst_column + (glyph.column - burst_column) * progress;
				row = burst_row + (glyph.row - burst_row) * progress;
				state.symbol.clear();
			}
			state.column = static_cast<int>(std::lround(column));
			state.row = static_cast<int>(std::lround(row));
			state.visible = local >= 0;
			const double final_position = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			const double fade = std::clamp(static_cast<double>(local - launch_frames
				- burst_frames) / settle_frames, 0.0, 1.0);
			state.color = mix(colors[shell], gradient(final_position), fade);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_laseretch(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<int> cell(static_cast<std::size_t>(canvas.width * canvas.height), -1);
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		const Glyph& glyph = glyphs[index];
		if (glyph.column < canvas.width && glyph.row < canvas.height)
			cell[static_cast<std::size_t>(glyph.row * canvas.width + glyph.column)]
				= static_cast<int>(index);
	}
	std::vector<unsigned char> visited(glyphs.size());
	std::vector<std::size_t> order;
	order.reserve(glyphs.size());
	while (order.size() < glyphs.size()) {
		std::size_t start = static_cast<std::size_t>(rng.below(glyphs.size()));
		while (visited[start] != 0)
			start = (start + 1) % glyphs.size();
		std::vector<std::size_t> stack = {start};
		visited[start] = 1;
		while (!stack.empty()) {
			const std::size_t current = stack.back();
			stack.pop_back();
			order.push_back(current);
			const Glyph& glyph = glyphs[current];
			std::vector<int> neighbors;
			const int dc[] = {1, -1, 0, 0};
			const int dr[] = {0, 0, 1, -1};
			for (int direction = 0; direction < 4; ++direction) {
				const int column = glyph.column + dc[direction];
				const int row = glyph.row + dr[direction];
				if (column < 0 || column >= canvas.width || row < 0 || row >= canvas.height)
					continue;
				const int neighbor = cell[static_cast<std::size_t>(row * canvas.width + column)];
				if (neighbor >= 0 && visited[static_cast<std::size_t>(neighbor)] == 0)
					neighbors.push_back(neighbor);
			}
			rng.shuffle(neighbors);
			for (int neighbor : neighbors) {
				const std::size_t next = static_cast<std::size_t>(neighbor);
				if (visited[next] == 0) {
					visited[next] = 1;
					stack.push_back(next);
				}
			}
		}
	}
	std::vector<int> activation(glyphs.size());
	for (std::size_t sequence = 0; sequence < order.size(); ++sequence)
		activation[order[sequence]] = static_cast<int>(sequence) * 2;
	const int cooling_frames = 24;
	const int frame_count = static_cast<int>(glyphs.size()) * 2 + cooling_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = local >= 0;
			state.symbol = local >= 0 && local < 3 ? "*" : std::string();
			const double vertical = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			const Color final_color = gradient(vertical);
			if (local < 3)
				state.color = mix({255, 255, 255}, {55, 108, 255},
					std::clamp(local / 3.0, 0.0, 1.0));
			else {
				const double cool = std::clamp(static_cast<double>(local - 3)
					/ cooling_frames, 0.0, 1.0);
				const Color hot = cool < 0.5
					? mix({255, 230, 128}, {255, 123, 0}, cool * 2.0)
					: mix({255, 123, 0}, final_color, (cool - 0.5) * 2.0);
				state.color = hot;
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_overflow(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const int cycles = rng.integer(2, 4);
	const int frames_per_row = 3;
	const int scrolling_frames = std::max(1, cycles * canvas.height * frames_per_row);
	const int settle_frames = 24;
	const int frame_count = scrolling_frames + settle_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const bool settling = frame > scrolling_frames;
		const int shift = frame / frames_per_row;
		const double settle = settling ? in_out_sine(
			static_cast<double>(frame - scrolling_frames) / settle_frames) : 0.0;
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int wrapped = ((glyph.row - shift) % canvas.height + canvas.height)
				% canvas.height;
			state.column = glyph.column;
			state.row = settling ? static_cast<int>(std::lround(
				wrapped + (glyph.row - wrapped) * settle)) : wrapped;
			state.visible = true;
			const double moving_position = canvas.height > 1
				? static_cast<double>(wrapped) / (canvas.height - 1) : 0.0;
			const double final_position = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			const Color moving = moving_position <= 0.5
				? mix({242, 235, 192}, {141, 191, 179}, moving_position * 2.0)
				: mix({141, 191, 179}, {242, 235, 192}, (moving_position - 0.5) * 2.0);
			state.color = settling ? mix(moving, gradient(final_position), settle) : moving;
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_rings(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const int ring_count = std::max(1, std::min(canvas.width / 4, canvas.height / 2));
	std::vector<int> ring(glyphs.size());
	std::vector<int> ordinal(glyphs.size());
	std::vector<int> counts(static_cast<std::size_t>(ring_count));
	std::vector<double> disperse_x(glyphs.size());
	std::vector<double> disperse_y(glyphs.size());
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		ring[index] = static_cast<int>(index % static_cast<std::size_t>(ring_count));
		ordinal[index] = counts[static_cast<std::size_t>(ring[index])]++;
		disperse_x[index] = rng.uniform(-3.0, 3.0);
		disperse_y[index] = rng.uniform(-2.0, 2.0);
	}
	std::vector<double> speed(static_cast<std::size_t>(ring_count));
	std::vector<int> direction(static_cast<std::size_t>(ring_count));
	for (int index = 0; index < ring_count; ++index) {
		speed[static_cast<std::size_t>(index)] = rng.uniform(0.65, 1.25);
		direction[static_cast<std::size_t>(index)] = rng.below(2) == 0 ? -1 : 1;
	}
	const int condense_frames = 32;
	const int spin_frames = 70;
	const int disperse_frames = 35;
	const int final_frames = 36;
	const int spin_one_end = condense_frames + spin_frames;
	const int disperse_end = spin_one_end + disperse_frames;
	const int spin_two_end = disperse_end + spin_frames;
	const int frame_count = spin_two_end + final_frames;
	const double center_column = (canvas.width - 1) / 2.0;
	const double center_row = (canvas.height - 1) / 2.0;
	const std::vector<Color> ring_colors = {{171, 72, 255}, {231, 178, 178}, {255, 254, 189}};
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int ring_index = ring[index];
			const int count = std::max(1, counts[static_cast<std::size_t>(ring_index)]);
			const double base_angle = 6.28318530717958647692 * ordinal[index] / count;
			double rotation = 0.0;
			if (frame > condense_frames) {
				const int spinning = std::min(frame - condense_frames, spin_frames)
					+ std::max(0, std::min(frame - disperse_end, spin_frames));
				rotation = direction[static_cast<std::size_t>(ring_index)]
					* speed[static_cast<std::size_t>(ring_index)]
					* 6.28318530717958647692 * spinning / spin_frames;
			}
			const double radius = ring_index + 1.0;
			double ring_column = center_column + std::cos(base_angle + rotation) * radius * 2.0;
			double ring_row = center_row + std::sin(base_angle + rotation) * radius;
			if (frame > spin_one_end && frame <= disperse_end) {
				const double progress = static_cast<double>(frame - spin_one_end) / disperse_frames;
				const double spread = std::sin(progress * 3.14159265358979323846);
				ring_column += disperse_x[index] * spread;
				ring_row += disperse_y[index] * spread;
			}
			if (frame <= condense_frames) {
				const double progress = in_out_sine(static_cast<double>(frame) / condense_frames);
				state.column = static_cast<int>(std::lround(
					glyph.column + (ring_column - glyph.column) * progress));
				state.row = static_cast<int>(std::lround(
					glyph.row + (ring_row - glyph.row) * progress));
			} else if (frame > spin_two_end) {
				const double progress = in_out_sine(static_cast<double>(frame - spin_two_end)
					/ final_frames);
				state.column = static_cast<int>(std::lround(
					ring_column + (glyph.column - ring_column) * progress));
				state.row = static_cast<int>(std::lround(
					ring_row + (glyph.row - ring_row) * progress));
			} else {
				state.column = static_cast<int>(std::lround(ring_column));
				state.row = static_cast<int>(std::lround(ring_row));
			}
			state.visible = true;
			const Color ring_color = ring_colors[static_cast<std::size_t>(ring_index)
				% ring_colors.size()];
			const double vertical = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			const double final_mix = std::clamp(static_cast<double>(frame - spin_two_end)
				/ final_frames, 0.0, 1.0);
			state.color = mix(ring_color, mix({171, 72, 255}, {255, 254, 189}, vertical),
				final_mix);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_smoke(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const int origin_column = rng.integer(0, canvas.width - 1);
	const int origin_row = rng.integer(0, canvas.height - 1);
	const std::vector<std::string> symbols = {"░", "▒", "▓", "▒", "░"};
	std::vector<int> activation(glyphs.size());
	int last_activation = 0;
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		activation[index] = (std::abs(glyphs[index].column - origin_column)
			+ std::abs(glyphs[index].row - origin_row)) * 2 + rng.integer(0, 4);
		last_activation = std::max(last_activation, activation[index]);
	}
	const int smoke_frames = 24;
	const int paint_frames = 18;
	const int frame_count = last_activation + smoke_frames + paint_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = true;
			const double vertical = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			const Color final_color = gradient(vertical);
			if (local < 0) {
				state.symbol.clear();
				state.color = {122, 122, 122};
			} else if (local < smoke_frames) {
				const std::size_t symbol = std::min(symbols.size() - 1,
					static_cast<std::size_t>(local * symbols.size() / smoke_frames));
				state.symbol = symbols[symbol];
				const double progress = static_cast<double>(local) / smoke_frames;
				state.color = progress < 0.5
					? mix({36, 36, 36}, {255, 255, 255}, progress * 2.0)
					: mix({255, 255, 255}, final_color, (progress - 0.5) * 2.0);
			} else {
				state.symbol.clear();
				const double paint = std::clamp(static_cast<double>(local - smoke_frames)
					/ paint_frames, 0.0, 1.0);
				state.color = mix({255, 255, 255}, final_color, paint);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_synthgrid(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<std::size_t> order(glyphs.size());
	std::vector<int> generation(glyphs.size());
	for (std::size_t index = 0; index < glyphs.size(); ++index)
		order[index] = index;
	rng.shuffle(order);
	const int per_frame = std::max(1, static_cast<int>(glyphs.size() / 20));
	for (std::size_t sequence = 0; sequence < order.size(); ++sequence)
		generation[order[sequence]] = static_cast<int>(sequence) / per_frame;
	const int expand_frames = 34;
	const int generation_frames = static_cast<int>(glyphs.size()) / per_frame + 18;
	const int collapse_frames = 24;
	const int frame_count = expand_frames + generation_frames + collapse_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			const double diagonal = canvas.width + canvas.height > 2
				? static_cast<double>(glyph.column + glyph.row)
					/ (canvas.width + canvas.height - 2) : 0.0;
			if (frame <= expand_frames) {
				const double reach = in_out_sine(static_cast<double>(frame) / expand_frames);
				const double distance = std::abs(glyph.column - (canvas.width - 1) / 2.0)
					/ std::max(1.0, (canvas.width - 1) / 2.0);
				state.visible = distance <= reach;
				state.symbol = glyph.row % 2 == 0 ? "─" : "│";
				state.color = mix({204, 0, 204}, {255, 255, 255}, diagonal);
			} else if (frame <= expand_frames + generation_frames) {
				const int local = frame - expand_frames - generation[index];
				state.visible = true;
				if (local < 0)
					state.symbol = glyph.row % 2 == 0 ? "─" : "│";
				else if (local < 12)
					state.symbol = local < 4 ? "░" : (local < 8 ? "▒" : "▓");
				else
					state.symbol.clear();
				state.color = local < 12
					? mix({204, 0, 204}, {255, 255, 255}, std::clamp(local / 12.0, 0.0, 1.0))
					: gradient(canvas.height > 1
						? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0);
			} else {
				state.visible = true;
				state.symbol.clear();
				state.color = gradient(canvas.height > 1
					? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_blackhole(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const double center_column = (canvas.width - 1) / 2.0;
	const double center_row = (canvas.height - 1) / 2.0;
	const double radius = std::max(2.0, std::min(canvas.width * 0.3, canvas.height * 0.3));
	std::vector<int> start_columns(glyphs.size());
	std::vector<int> start_rows(glyphs.size());
	std::vector<double> angle(glyphs.size());
	std::vector<double> explosion(glyphs.size());
	std::vector<std::string> star(glyphs.size());
	const std::vector<std::string> stars = {"*", "'", "`", "¤", "•", "°", "·"};
	const std::vector<Color> star_colors = {
		{255, 204, 13}, {255, 115, 38}, {255, 25, 77}, {191, 38, 105}, {112, 42, 140}, {4, 157, 191}
	};
	std::vector<Color> color(glyphs.size());
	std::vector<std::size_t> selection(glyphs.size());
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		start_columns[index] = rng.integer(0, canvas.width - 1);
		start_rows[index] = rng.integer(0, canvas.height - 1);
		angle[index] = rng.uniform(0.0, 6.28318530717958647692);
		explosion[index] = rng.uniform(2.0, 5.0);
		star[index] = stars[static_cast<std::size_t>(rng.below(stars.size()))];
		color[index] = star_colors[static_cast<std::size_t>(rng.below(star_colors.size()))];
		selection[index] = index;
	}
	rng.shuffle(selection);
	std::vector<unsigned char> ring_member(glyphs.size());
	const std::size_t ring_size = std::min(glyphs.size(),
		static_cast<std::size_t>(std::max(3.0, radius * 3.0)));
	for (std::size_t index = 0; index < ring_size; ++index)
		ring_member[selection[index]] = 1;
	const int forming_frames = 38;
	const int consuming_frames = 46;
	const int collapsing_frames = 18;
	const int exploding_frames = 20;
	const int settle_frames = 34;
	const int consume_end = forming_frames + consuming_frames;
	const int collapse_end = consume_end + collapsing_frames;
	const int explode_end = collapse_end + exploding_frames;
	const int frame_count = explode_end + settle_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			double column = start_columns[index];
			double row = start_rows[index];
			state.visible = true;
			state.symbol = star[index];
			state.color = mix({74, 74, 77}, {255, 255, 255}, rng.random() * 0.25);
			const double ring_angle = angle[index] + frame * 0.045;
			const double ring_column = center_column + std::cos(ring_angle) * radius;
			const double ring_row = center_row + std::sin(ring_angle) * radius * 0.5;
			if (frame <= forming_frames && ring_member[index] != 0) {
				const double progress = in_out_sine(static_cast<double>(frame) / forming_frames);
				column += (ring_column - column) * progress;
				row += (ring_row - row) * progress;
				state.symbol = "*";
				state.color = {255, 255, 255};
			} else if (frame <= consume_end) {
				if (ring_member[index] != 0) {
					column = ring_column;
					row = ring_row;
					state.symbol = "*";
					state.color = {255, 255, 255};
				} else {
					const double progress = in_out_expo(static_cast<double>(frame - forming_frames)
						/ consuming_frames);
					column += (center_column - column) * progress;
					row += (center_row - row) * progress;
					state.color = mix(color[index], {0, 0, 0}, progress);
					state.visible = progress < 0.98;
				}
			} else if (frame <= collapse_end) {
				const double progress = in_quad(static_cast<double>(frame - consume_end)
					/ collapsing_frames);
				column = center_column + (ring_column - center_column) * (1.0 - progress);
				row = center_row + (ring_row - center_row) * (1.0 - progress);
				state.visible = ring_member[index] != 0;
				state.symbol = "*";
				state.color = {255, 255, 255};
			} else if (frame <= explode_end) {
				const double progress = out_expo(static_cast<double>(frame - collapse_end)
					/ exploding_frames);
				column = center_column + std::cos(angle[index]) * explosion[index] * progress;
				row = center_row + std::sin(angle[index]) * explosion[index] * 0.6 * progress;
				state.visible = true;
				state.symbol.clear();
				state.color = color[index];
			} else {
				const double progress = in_out_quart(static_cast<double>(frame - explode_end)
					/ settle_frames);
				const double exploded_column = center_column + std::cos(angle[index]) * explosion[index];
				const double exploded_row = center_row + std::sin(angle[index]) * explosion[index] * 0.6;
				column = exploded_column + (glyph.column - exploded_column) * progress;
				row = exploded_row + (glyph.row - exploded_row) * progress;
				state.symbol.clear();
				const double diagonal = canvas.width + canvas.height > 2
					? static_cast<double>(glyph.column + glyph.row)
						/ (canvas.width + canvas.height - 2) : 0.0;
				state.color = mix(color[index], gradient(diagonal), progress);
			}
			state.column = static_cast<int>(std::lround(column));
			state.row = static_cast<int>(std::lround(row));
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_crumble(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<std::size_t> order(glyphs.size());
	std::vector<int> activation(glyphs.size());
	std::vector<std::string> dust(glyphs.size());
	const std::vector<std::string> dust_symbols = {"*", ".", ","};
	for (std::size_t index = 0; index < glyphs.size(); ++index)
		order[index] = index;
	rng.shuffle(order);
	const int per_frame = std::max(1, static_cast<int>(glyphs.size() / 30));
	for (std::size_t sequence = 0; sequence < order.size(); ++sequence)
		activation[order[sequence]] = static_cast<int>(sequence) / per_frame;
	for (std::size_t index = 0; index < glyphs.size(); ++index)
		dust[index] = dust_symbols[static_cast<std::size_t>(rng.below(dust_symbols.size()))];
	const int falling_frames = static_cast<int>(glyphs.size()) / per_frame + 34;
	const int vacuum_frames = 38;
	const int reset_frames = 36;
	const int vacuum_end = falling_frames + vacuum_frames;
	const int frame_count = vacuum_end + reset_frames;
	const double center_column = (canvas.width - 1) / 2.0;
	const double center_row = (canvas.height - 1) / 2.0;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.visible = true;
			if (frame <= falling_frames) {
				const int local = frame - activation[index];
				const double progress = out_bounce(std::clamp(local / 30.0, 0.0, 1.0));
				state.column = glyph.column;
				state.row = static_cast<int>(std::lround(
					glyph.row + (canvas.height - 1 - glyph.row) * progress));
				state.symbol = local >= 0 && local < 30 ? dust[index] : std::string();
				const double diagonal = canvas.width + canvas.height > 2
					? static_cast<double>(glyph.column + glyph.row)
						/ (canvas.width + canvas.height - 2) : 0.0;
				state.color = mix(mix({92, 225, 255}, {255, 140, 0}, diagonal),
					{80, 80, 80}, progress * 0.55);
			} else if (frame <= vacuum_end) {
				const double progress = in_out_circ(static_cast<double>(frame - falling_frames)
					/ vacuum_frames);
				const double start_column = glyph.column;
				const double start_row = canvas.height - 1;
				const double one_minus = 1.0 - progress;
				state.column = static_cast<int>(std::lround(one_minus * one_minus * start_column
					+ 2.0 * one_minus * progress * center_column + progress * progress * glyph.column));
				state.row = static_cast<int>(std::lround(one_minus * one_minus * start_row
					+ 2.0 * one_minus * progress * center_row + progress * progress * -1.0));
				state.symbol = dust[index];
				state.color = {100, 100, 100};
			} else {
				const double progress = in_out_sine(static_cast<double>(frame - vacuum_end)
					/ reset_frames);
				state.column = glyph.column;
				state.row = static_cast<int>(std::lround(-1.0 + (glyph.row + 1.0) * progress));
				state.symbol = progress < 0.8 ? dust[index] : std::string();
				const double diagonal = canvas.width + canvas.height > 2
					? static_cast<double>(glyph.column + glyph.row)
						/ (canvas.width + canvas.height - 2) : 0.0;
				const Color final_color = mix({92, 225, 255}, {255, 140, 0}, diagonal);
				state.color = mix({255, 255, 255}, final_color, progress);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_matrix(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::vector<std::string> symbols = {
		"2", "5", "9", "8", "Z", "*", ")", ":", ".", "=", "+", "-", "|", "_",
		"ｦ", "ｱ", "ｳ", "ｴ", "ｵ", "ｶ", "ｷ", "ｹ", "ｺ", "ｻ", "ｼ", "ｽ", "ｾ", "ｿ"
	};
	std::vector<int> column_delay(static_cast<std::size_t>(canvas.width));
	std::vector<int> column_speed(static_cast<std::size_t>(canvas.width));
	std::vector<int> column_length(static_cast<std::size_t>(canvas.width));
	for (int column = 0; column < canvas.width; ++column) {
		column_delay[static_cast<std::size_t>(column)] = rng.integer(0, 24);
		column_speed[static_cast<std::size_t>(column)] = rng.integer(1, 3);
		column_length[static_cast<std::size_t>(column)] = rng.integer(2,
			std::max(2, canvas.height));
	}
	const int rain_frames = 90;
	const int fill_frames = 32;
	const int resolve_frames = 42;
	const int fill_end = rain_frames + fill_frames;
	const int frame_count = fill_end + resolve_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			if (frame <= rain_frames) {
				const int delay = column_delay[static_cast<std::size_t>(glyph.column)];
				const int cycle = std::max(1, canvas.height + column_length[static_cast<std::size_t>(glyph.column)]);
				const int head = frame < delay ? -100
					: ((frame - delay) / column_speed[static_cast<std::size_t>(glyph.column)]) % cycle;
				const int behind = head - glyph.row;
				state.visible = behind >= 0
					&& behind < column_length[static_cast<std::size_t>(glyph.column)];
				state.symbol = symbols[static_cast<std::size_t>(rng.below(symbols.size()))];
				const double trail = column_length[static_cast<std::size_t>(glyph.column)] > 1
					? static_cast<double>(behind) / (column_length[static_cast<std::size_t>(glyph.column)] - 1)
					: 0.0;
				state.color = behind == 0 ? Color{219, 255, 219}
					: mix({146, 190, 146}, {24, 83, 24}, std::clamp(trail, 0.0, 1.0));
			} else if (frame <= fill_end) {
				const double threshold = static_cast<double>(frame - rain_frames)
					/ fill_frames * canvas.height;
				state.visible = glyph.row <= threshold;
				state.symbol = symbols[static_cast<std::size_t>(rng.below(symbols.size()))];
				state.color = mix({219, 255, 219}, {24, 83, 24}, canvas.height > 1
					? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0);
			} else {
				const double threshold = static_cast<double>(frame - fill_end)
					/ resolve_frames * (canvas.width + canvas.height - 2);
				const double local = std::clamp(threshold - (glyph.column + glyph.row) + 1.0,
					0.0, 1.0);
				state.visible = true;
				state.symbol = local >= 1.0 ? std::string()
					: symbols[static_cast<std::size_t>(rng.below(symbols.size()))];
				const double radial = std::hypot(glyph.column - (canvas.width - 1) / 2.0,
					glyph.row - (canvas.height - 1) / 2.0);
				const double maximum = std::max(1.0, std::hypot((canvas.width - 1) / 2.0,
					(canvas.height - 1) / 2.0));
				state.color = mix({146, 190, 146}, mix({146, 190, 146}, {51, 107, 51},
					radial / maximum), local);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_orbittingvolley(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	const int volley_size = std::max(1, static_cast<int>(glyphs.size() * 0.03));
	const int launch_delay = 8;
	const int travel_frames = 30;
	std::vector<int> activation(glyphs.size());
	for (std::size_t index = 0; index < glyphs.size(); ++index)
		activation[index] = static_cast<int>(index) / (volley_size * 4) * launch_delay;
	const int frame_count = activation.back() + travel_frames + 10;
	const int perimeter = std::max(1, canvas.width * 2 + canvas.height * 2 - 4);
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const int local = frame - activation[index];
			const int launcher = static_cast<int>(index % 4);
			const int perimeter_position = (activation[index] * 2 + launcher * perimeter / 4) % perimeter;
			int launch_column;
			int launch_row;
			if (perimeter_position < canvas.width) {
				launch_column = perimeter_position;
				launch_row = 0;
			} else if (perimeter_position < canvas.width + canvas.height - 1) {
				launch_column = canvas.width - 1;
				launch_row = perimeter_position - canvas.width + 1;
			} else if (perimeter_position < canvas.width * 2 + canvas.height - 2) {
				launch_column = canvas.width - 2
					- (perimeter_position - (canvas.width + canvas.height - 1));
				launch_row = canvas.height - 1;
			} else {
				launch_column = 0;
				launch_row = canvas.height - 2
					- (perimeter_position - (canvas.width * 2 + canvas.height - 2));
			}
			const double progress = in_out_sine(std::clamp(
				static_cast<double>(local) / travel_frames, 0.0, 1.0));
			state.column = static_cast<int>(std::lround(
				launch_column + (glyph.column - launch_column) * progress));
			state.row = static_cast<int>(std::lround(
				launch_row + (glyph.row - launch_row) * progress));
			state.visible = local >= 0;
			state.symbol = local >= 0 && local < 3 ? "█" : std::string();
			const double radial = std::hypot(glyph.column - (canvas.width - 1) / 2.0,
				glyph.row - (canvas.height - 1) / 2.0);
			const double maximum = std::max(1.0, std::hypot((canvas.width - 1) / 2.0,
				(canvas.height - 1) / 2.0));
			state.color = mix({255, 161, 92}, {68, 212, 146}, radial / maximum);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_spotlights(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const int spotlight_count = 3;
	std::vector<double> phases(static_cast<std::size_t>(spotlight_count));
	for (double& phase : phases)
		phase = rng.uniform(0.0, 6.28318530717958647692);
	const int search_frames = 100;
	const int converge_frames = 26;
	const int expand_frames = 38;
	const int converge_end = search_frames + converge_frames;
	const int frame_count = converge_end + expand_frames;
	const double center_column = (canvas.width - 1) / 2.0;
	const double center_row = (canvas.height - 1) / 2.0;
	const double beam_width = std::max(1.5, std::min(canvas.width, canvas.height) / 2.0);
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		std::vector<double> light_columns(static_cast<std::size_t>(spotlight_count));
		std::vector<double> light_rows(static_cast<std::size_t>(spotlight_count));
		for (int light = 0; light < spotlight_count; ++light) {
			const double moving_column = center_column
				+ std::sin(frame * (0.055 + light * 0.008) + phases[static_cast<std::size_t>(light)])
					* center_column;
			const double moving_row = center_row
				+ std::cos(frame * (0.043 + light * 0.006) + phases[static_cast<std::size_t>(light)])
					* center_row;
			if (frame <= search_frames) {
				light_columns[static_cast<std::size_t>(light)] = moving_column;
				light_rows[static_cast<std::size_t>(light)] = moving_row;
			} else {
				const double progress = in_out_sine(std::clamp(
					static_cast<double>(frame - search_frames) / converge_frames, 0.0, 1.0));
				light_columns[static_cast<std::size_t>(light)]
					= moving_column + (center_column - moving_column) * progress;
				light_rows[static_cast<std::size_t>(light)]
					= moving_row + (center_row - moving_row) * progress;
			}
		}
		const double expansion = frame > converge_end
			? static_cast<double>(frame - converge_end) / expand_frames : 0.0;
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = true;
			double distance = 1e9;
			for (int light = 0; light < spotlight_count; ++light)
				distance = std::min(distance, std::hypot(
					glyph.column - light_columns[static_cast<std::size_t>(light)],
					glyph.row - light_rows[static_cast<std::size_t>(light)]));
			const double active_width = frame > converge_end
				? beam_width + expansion * std::max(canvas.width, canvas.height) : beam_width;
			const double brightness = expansion >= 1.0 ? 1.0
				: std::clamp(1.0 - distance / active_width, 0.2, 1.0);
			const double vertical = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			const Color base = mix({171, 72, 255}, {255, 254, 189}, vertical);
			state.color = mix({0, 0, 0}, base, brightness);
			state.symbol = distance < 0.45 && frame <= converge_end ? "O" : std::string();
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_swarm(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::size_t swarm_size = std::max<std::size_t>(1, glyphs.size() / 10);
	const std::size_t swarm_count = (glyphs.size() + swarm_size - 1) / swarm_size;
	std::vector<int> spawn_columns(swarm_count);
	std::vector<int> spawn_rows(swarm_count);
	std::vector<int> area_columns(swarm_count * 3);
	std::vector<int> area_rows(swarm_count * 3);
	std::vector<double> offset_columns(glyphs.size());
	std::vector<double> offset_rows(glyphs.size());
	for (std::size_t swarm = 0; swarm < swarm_count; ++swarm) {
		spawn_columns[swarm] = rng.integer(0, canvas.width - 1);
		spawn_rows[swarm] = rng.integer(0, canvas.height - 1);
		for (int area = 0; area < 3; ++area) {
			area_columns[swarm * 3 + static_cast<std::size_t>(area)]
				= rng.integer(0, canvas.width - 1);
			area_rows[swarm * 3 + static_cast<std::size_t>(area)]
				= rng.integer(0, canvas.height - 1);
		}
	}
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		offset_columns[index] = rng.uniform(-2.0, 2.0);
		offset_rows[index] = rng.uniform(-1.0, 1.0);
	}
	const int leg_frames = 24;
	const int launch_gap = 8;
	const int legs = 4;
	const int frame_count = static_cast<int>(swarm_count - 1) * launch_gap
		+ leg_frames * legs;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			const std::size_t swarm = index / swarm_size;
			const int local = frame - static_cast<int>(swarm) * launch_gap;
			const int leg = std::clamp(local / leg_frames, 0, legs - 1);
			const double progress = in_out_sine(std::clamp(
				static_cast<double>(local - leg * leg_frames) / leg_frames, 0.0, 1.0));
			double from_column = spawn_columns[swarm];
			double from_row = spawn_rows[swarm];
			double to_column;
			double to_row;
			if (leg > 0) {
				from_column = area_columns[swarm * 3 + static_cast<std::size_t>(leg - 1)];
				from_row = area_rows[swarm * 3 + static_cast<std::size_t>(leg - 1)];
			}
			if (leg < 3) {
				to_column = area_columns[swarm * 3 + static_cast<std::size_t>(leg)];
				to_row = area_rows[swarm * 3 + static_cast<std::size_t>(leg)];
			} else {
				to_column = glyph.column;
				to_row = glyph.row;
			}
			const double cohesion = leg < 3 ? 1.0 : 1.0 - progress;
			state.column = static_cast<int>(std::lround(from_column
				+ (to_column - from_column) * progress + offset_columns[index] * cohesion));
			state.row = static_cast<int>(std::lround(from_row
				+ (to_row - from_row) * progress + offset_rows[index] * cohesion));
			state.visible = local >= 0;
			state.symbol.clear();
			const double flash = std::sin(progress * 3.14159265358979323846);
			const double horizontal = canvas.width > 1
				? static_cast<double>(glyph.column) / (canvas.width - 1) : 0.0;
			const Color final_color = mix({49, 185, 0}, {240, 255, 101}, horizontal);
			state.color = leg < 3 ? mix({49, 160, 212}, {242, 234, 121}, flash)
				: mix({242, 234, 121}, final_color, progress);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_thunderstorm(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::vector<std::string> rain_symbols = {"\\", ".", ","};
	const std::vector<std::string> spark_symbols = {"*", ".", "'"};
	const int fade_frames = 24;
	const int storm_frames = 130;
	const int restore_frames = 34;
	const int storm_end = fade_frames + storm_frames;
	const int frame_count = storm_end + restore_frames;
	const std::vector<int> strike_frames = {24, 61, 99};
	std::vector<int> strike_columns(strike_frames.size());
	for (int& column : strike_columns)
		column = rng.integer(0, canvas.width - 1);
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		const int storm_frame = frame - fade_frames;
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.column = glyph.column;
			state.row = glyph.row;
			state.visible = true;
			const double vertical = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			const Color final_color = gradient(vertical);
			if (frame <= fade_frames) {
				const double progress = static_cast<double>(frame) / fade_frames;
				state.symbol.clear();
				state.color = mix(final_color, {24, 29, 45}, progress);
			} else if (frame <= storm_end) {
				state.symbol.clear();
				state.color = {24, 29, 45};
				const int rain_phase = (storm_frame * 2 + glyph.column * 3 - glyph.row) % 13;
				if (rain_phase >= 0 && rain_phase < 3) {
					state.symbol = rain_symbols[static_cast<std::size_t>(
						rng.below(rain_symbols.size()))];
					state.color = {170, 170, 255};
				}
				for (std::size_t strike = 0; strike < strike_frames.size(); ++strike) {
					const int local = storm_frame - strike_frames[strike];
					const int strike_column = strike_columns[strike]
						+ static_cast<int>(std::lround(std::sin(glyph.row * 1.7 + strike) * 1.2));
					const int distance = std::abs(glyph.column - strike_column);
					if (local >= glyph.row && local < glyph.row + 5 && distance == 0) {
						state.symbol = glyph.row % 3 == 0 ? "|" : (glyph.row % 3 == 1 ? "/" : "\\");
						state.color = {104, 163, 232};
					} else if (local >= canvas.height && local < canvas.height + 14
						&& distance <= 2) {
						const double cool = static_cast<double>(local - canvas.height) / 14.0;
						state.symbol = distance == 0 && local < canvas.height + 5
							? spark_symbols[static_cast<std::size_t>(rng.below(spark_symbols.size()))]
							: std::string();
						state.color = mix({239, 84, 17}, {24, 29, 45}, cool);
					}
				}
			} else {
				const double progress = in_out_sine(static_cast<double>(frame - storm_end)
					/ restore_frames);
				state.symbol.clear();
				state.color = mix({24, 29, 45}, final_color, progress);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_unstable(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	std::vector<Glyph> jumbled = glyphs;
	rng.shuffle(jumbled);
	std::vector<int> edge_columns(glyphs.size());
	std::vector<int> edge_rows(glyphs.size());
	std::vector<double> phase(glyphs.size());
	for (std::size_t index = 0; index < glyphs.size(); ++index) {
		const int edge = rng.integer(0, 3);
		if (edge == 0) {
			edge_columns[index] = 0;
			edge_rows[index] = rng.integer(0, canvas.height - 1);
		} else if (edge == 1) {
			edge_columns[index] = canvas.width - 1;
			edge_rows[index] = rng.integer(0, canvas.height - 1);
		} else if (edge == 2) {
			edge_columns[index] = rng.integer(0, canvas.width - 1);
			edge_rows[index] = 0;
		} else {
			edge_columns[index] = rng.integer(0, canvas.width - 1);
			edge_rows[index] = canvas.height - 1;
		}
		phase[index] = rng.uniform(0.0, 6.28318530717958647692);
	}
	const int rumble_frames = 90;
	const int explosion_frames = 28;
	const int hold_frames = 22;
	const int reassembly_frames = 40;
	const int explosion_end = rumble_frames + explosion_frames;
	const int hold_end = explosion_end + hold_frames;
	const int frame_count = hold_end + reassembly_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			double column;
			double row;
			double final_mix = 0.0;
			if (frame <= rumble_frames) {
				const double intensity = static_cast<double>(frame) / rumble_frames;
				column = jumbled[index].column
					+ std::sin(frame * 0.9 + phase[index]) * intensity * 1.5;
				row = jumbled[index].row
					+ std::cos(frame * 1.1 + phase[index]) * intensity;
			} else if (frame <= explosion_end) {
				const double progress = out_expo(static_cast<double>(frame - rumble_frames)
					/ explosion_frames);
				column = jumbled[index].column
					+ (edge_columns[index] - jumbled[index].column) * progress;
				row = jumbled[index].row
					+ (edge_rows[index] - jumbled[index].row) * progress;
			} else if (frame <= hold_end) {
				column = edge_columns[index];
				row = edge_rows[index];
			} else {
				const double progress = out_expo(static_cast<double>(frame - hold_end)
					/ reassembly_frames);
				column = edge_columns[index] + (glyph.column - edge_columns[index]) * progress;
				row = edge_rows[index] + (glyph.row - edge_rows[index]) * progress;
				final_mix = progress;
			}
			state.column = static_cast<int>(std::lround(column));
			state.row = static_cast<int>(std::lround(row));
			state.visible = true;
			state.symbol.clear();
			const double vertical = canvas.height > 1
				? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
			const Color final_color = gradient(vertical);
			const double instability = frame <= rumble_frames
				? static_cast<double>(frame) / rumble_frames : 1.0;
			state.color = frame > hold_end ? mix({255, 146, 0}, final_color, final_mix)
				: mix(final_color, {255, 146, 0}, instability);
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

bool run_vhstape(const std::vector<Glyph>& glyphs, const EffectOptions& options,
	TerminalSession& terminal, const volatile sig_atomic_t& interrupted,
	const volatile sig_atomic_t& resized)
{
	if (glyphs.empty())
		return true;
	const CanvasSize canvas = canvas_size(glyphs);
	Rng rng = effect_rng(options);
	const std::vector<Color> glitch_colors = {
		{255, 255, 255}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}
	};
	const std::vector<Color> noise_colors = {
		{30, 30, 31}, {60, 59, 61}, {109, 108, 112}, {162, 161, 166}, {203, 201, 207}, {255, 255, 255}
	};
	const std::vector<std::string> noise_symbols = {"#", "*", ".", ":"};
	const int glitch_frames = 120;
	const int noise_frames = 34;
	const int redraw_frames = 40;
	const int noise_end = glitch_frames + noise_frames;
	const int frame_count = noise_end + redraw_frames;
	const auto duration = frame_duration(options);
	std::vector<CharacterState> states(glyphs.size());
	std::vector<int> row_offset(static_cast<std::size_t>(canvas.height));
	std::vector<int> row_color(static_cast<std::size_t>(canvas.height));
	for (int frame = 0; frame <= frame_count; ++frame) {
		if (should_stop(interrupted, resized))
			return false;
		const auto started = std::chrono::steady_clock::now();
		if (frame <= glitch_frames) {
			for (int row = 0; row < canvas.height; ++row) {
				if (rng.random() < 0.08) {
					row_offset[static_cast<std::size_t>(row)] = rng.integer(-6, 6);
					row_color[static_cast<std::size_t>(row)] = rng.integer(0,
						static_cast<int>(glitch_colors.size()) - 1);
				} else if (rng.random() < 0.18) {
					row_offset[static_cast<std::size_t>(row)] = 0;
				}
			}
		}
		for (std::size_t index = 0; index < glyphs.size(); ++index) {
			const Glyph& glyph = glyphs[index];
			CharacterState& state = states[index];
			state.visible = true;
			if (frame <= glitch_frames) {
				int offset = row_offset[static_cast<std::size_t>(glyph.row)];
				const int wave_center = canvas.height > 0
					? (frame / 3) % canvas.height : 0;
				if (std::abs(glyph.row - wave_center) <= 1)
					offset += 8 - std::abs(glyph.row - wave_center) * 3;
				state.column = glyph.column + offset;
				state.row = glyph.row;
				state.symbol.clear();
				state.color = glitch_colors[static_cast<std::size_t>(
					row_color[static_cast<std::size_t>(glyph.row)])];
				if (rng.random() < 0.012) {
					state.symbol = noise_symbols[static_cast<std::size_t>(rng.below(noise_symbols.size()))];
					state.color = noise_colors[static_cast<std::size_t>(rng.below(noise_colors.size()))];
				}
			} else if (frame <= noise_end) {
				state.column = glyph.column;
				state.row = glyph.row;
				state.symbol = noise_symbols[static_cast<std::size_t>(rng.below(noise_symbols.size()))];
				state.color = noise_colors[static_cast<std::size_t>(rng.below(noise_colors.size()))];
			} else {
				const double threshold = static_cast<double>(frame - noise_end)
					/ redraw_frames * (canvas.width + canvas.height - 2);
				const double local = std::clamp(threshold - (glyph.column + glyph.row) + 1.0,
					0.0, 1.0);
				state.column = glyph.column;
				state.row = glyph.row;
				state.symbol = local < 1.0 ? "█" : std::string();
				const double vertical = canvas.height > 1
					? static_cast<double>(glyph.row) / (canvas.height - 1) : 0.0;
				state.color = mix({255, 255, 255},
					mix({171, 72, 255}, {255, 254, 189}, vertical), local);
			}
		}
		render(glyphs, states, canvas, options.color, terminal);
		finish_frame(started, duration);
	}
	return true;
}

} // namespace ttfx
