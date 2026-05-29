#include <platform.h>
#include <uart.h>
#include <vga.h>
#include <stdio.h>

#define PRINT_TO_VGA 0

void platform_init() {
    uart_init((volatile char*)UART_BASE);
}

void _putchar(char c) {
    uart_putc((volatile char*)UART_BASE, c);
#if PRINT_TO_VGA
    vga_putc(c);
#endif
}

int putchar(int c) {
    _putchar(c);
    return 0;
}

int puts(const char *s) {
    uart_puts((volatile char*)UART_BASE, s);
    uart_putc((volatile char*)UART_BASE, (int) '\n');

#if PRINT_TO_VGA
    vga_puts(s);
    vga_putc((int)'\n');
#endif
    return 0;
}

int getchar(void) {
    return uart_getc((volatile char*)UART_BASE);
}
