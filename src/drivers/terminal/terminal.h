#include <limine.h>
#include <drivers/terminal/flanterm/flanterm.h>
#include <drivers/terminal/flanterm/backends/fb.h>

void setup_terminal(struct limine_framebuffer *framebuffer);
void terminal_write(const char *msg, size_t count);
void terminal_write_char(const char c);
