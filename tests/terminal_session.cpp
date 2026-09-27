#include "terminal.h"

int main()
{
	ttfx::TerminalSession terminal(true);
	terminal.draw("first\n", 1);
	terminal.draw("second\n", 1);
	terminal.draw("top\nbottom\n", 2);
	terminal.draw("new top\nnew bottom\n", 2);
	return 0;
}
