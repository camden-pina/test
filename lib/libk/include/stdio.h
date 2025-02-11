#ifndef _KRNL_STDIO_H
#define _KRNL_STDIO_H 1

#include <stdarg.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

void putc(char ch);
void printf(const char* fmt, ...);

void DEBUG(const char* fmt, ...);
void PANIC(const char* fmt, ...);

void drawRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned long long color);
void draw_pixel(unsigned int x, unsigned int y, unsigned long long color);

void krnl_set_graphics_ouutput_protocol(unsigned int* fb, unsigned long long width, unsigned long long height, unsigned long long bpp, unsigned long long pixelsperscanline);

unsigned int* krnl_get_framebuffer(void);
void krnl_printf_reset_x(void);
void krnl_printf_reset_y(void);

#ifdef __cplusplus
}
#endif

#endif
