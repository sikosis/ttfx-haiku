#ifndef TTFX_TERMINAL_H
#define TTFX_TERMINAL_H

#include <cstdint>
#include <string>
#include <vector>

namespace ttfx {

struct Size {
	int columns = 80;
	int rows = 24;
};

struct Glyph {
	std::string text;
	int column = 0;
	int row = 0;
};

Size terminal_size();
bool stdout_is_terminal();
bool stdin_is_terminal();
std::vector<Glyph> layout_utf8(const std::string& input);

class TerminalSession {
public:
	explicit TerminalSession(bool enabled);
	~TerminalSession();

	TerminalSession(const TerminalSession&) = delete;
	TerminalSession& operator=(const TerminalSession&) = delete;

	void draw(const std::string& frame, int height);

private:
	bool enabled_;
	bool first_frame_ = true;
	int previous_height_ = 0;
};

std::string rgb(int red, int green, int blue);
std::string reset_color();

} // namespace ttfx

#endif
