#include <limine.h>
#include <string.h>
#include <drivers/terminal/flanterm/flanterm.h>
#include <drivers/terminal/flanterm/backends/fb.h>
#include <drivers/terminal/terminal.h>

struct flanterm_context *ft_ctx;

void terminal_write_char(const char c) {
        flanterm_putchar_wrapper(ft_ctx,c);
}

void terminal_write(const char *msg, size_t count) {
        flanterm_write(ft_ctx, msg, count);
}

void setup_terminal(struct limine_framebuffer *framebuffer) {
    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch,
        8, 16,
        8, 8,
        8, 0,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    ); 
}
