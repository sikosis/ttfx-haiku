#include "terminal.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace ttfx {
namespace {

int positive_environment_value(const char* name)
{
	const char* value = std::getenv(name);
	if (value == nullptr || *value == '\0')
		return 0;
	char* end = nullptr;
	errno = 0;
	const long parsed = std::strtol(value, &end, 10);
	if (errno != 0 || *end != '\0' || parsed <= 0 || parsed > 100000)
		return 0;
	return static_cast<int>(parsed);
}

std::size_t utf8_sequence_length(unsigned char first)
{
	if (first < 0x80)
		return 1;
	if ((first & 0xe0) == 0xc0)
		return 2;
	if ((first & 0xf0) == 0xe0)
		return 3;
	if ((first & 0xf8) == 0xf0)
		return 4;
	throw std::runtime_error("input is not valid UTF-8");
}

void validate_utf8_sequence(const std::string& input, std::size_t offset, std::size_t length)
{
	std::uint32_t codepoint = 0;
	if (length == 1)
		return;
	if (length == 2)
		codepoint = static_cast<unsigned char>(input[offset]) & 0x1f;
	else if (length == 3)
		codepoint = static_cast<unsigned char>(input[offset]) & 0x0f;
	else
		codepoint = static_cast<unsigned char>(input[offset]) & 0x07;

	for (std::size_t index = 1; index < length; ++index) {
		const unsigned char continuation = static_cast<unsigned char>(input[offset + index]);
		if ((continuation & 0xc0) != 0x80)
			throw std::runtime_error("input is not valid UTF-8");
		codepoint = (codepoint << 6) | (continuation & 0x3f);
	}
	const std::uint32_t minimum = length == 2 ? 0x80 : (length == 3 ? 0x800 : 0x10000);
	if (codepoint < minimum || codepoint > 0x10ffff
		|| (codepoint >= 0xd800 && codepoint <= 0xdfff))
		throw std::runtime_error("input is not valid UTF-8");
}

} // namespace

Size terminal_size()
{
	Size result;
	const int environment_columns = positive_environment_value("COLUMNS");
	const int environment_rows = positive_environment_value("LINES");

	struct winsize window = {};
#ifdef __HAIKU__
	const bool queried = tcgetwinsize(STDOUT_FILENO, &window) == 0;
#else
	const bool queried = ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0;
#endif
	if (queried && window.ws_col > 0)
		result.columns = window.ws_col;
	if (queried && window.ws_row > 0)
		result.rows = window.ws_row;
	if (environment_columns > 0)
		result.columns = environment_columns;
	if (environment_rows > 0)
		result.rows = environment_rows;
	return result;
}

bool stdout_is_terminal()
{
	return isatty(STDOUT_FILENO) != 0;
}

bool stdin_is_terminal()
{
	return isatty(STDIN_FILENO) != 0;
}

std::vector<Glyph> layout_utf8(const std::string& input)
{
	std::vector<Glyph> glyphs;
	int row = 0;
	int column = 0;
	for (std::size_t offset = 0; offset < input.size();) {
		const unsigned char first = static_cast<unsigned char>(input[offset]);
		if (first == '\n') {
			++row;
			column = 0;
			++offset;
			continue;
		}
		if (first == '\r') {
			++offset;
			continue;
		}

		const std::size_t length = utf8_sequence_length(first);
		if (offset + length > input.size())
			throw std::runtime_error("input ends inside a UTF-8 sequence");
		validate_utf8_sequence(input, offset, length);
		glyphs.push_back({input.substr(offset, length), column, row});
		offset += length;
		++column;
	}
	return glyphs;
}

TerminalSession::TerminalSession(bool enabled)
	: enabled_(enabled)
{
	if (enabled_)
		std::cout << "\x1b[?25l";
}

TerminalSession::~TerminalSession()
{
	if (enabled_)
		std::cout << reset_color() << "\x1b[?25h" << std::flush;
}

void TerminalSession::draw(const std::string& frame, int height)
{
	// Every rendered row ends with a newline, so stdout is one line below the
	// previous frame. Move up the full frame height before repainting it. The
	// old height - 1 calculation made one-line animations scroll forever.
	if (enabled_ && !first_frame_)
		std::cout << "\x1b[" << previous_height_ << 'A';
	std::cout << frame << std::flush;
	first_frame_ = false;
	previous_height_ = std::max(height, 1);
}

std::string rgb(int red, int green, int blue)
{
	return "\x1b[38;2;" + std::to_string(red) + ';' + std::to_string(green) + ';'
		+ std::to_string(blue) + 'm';
}

std::string reset_color()
{
	return "\x1b[0m";
}

} // namespace ttfx
