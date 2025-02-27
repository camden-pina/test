#include <stdio.h>
#include <string.h>
#include <font.h>

struct graphics_putput_protocol
{
    unsigned int* framebuffer;
    unsigned long long width;
    unsigned long long height;
    unsigned long long bpp;
    unsigned long long pixelsperscanline;
};

struct graphics_putput_protocol GOP;

void drawRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, unsigned long long color)
{
	for (unsigned long long i = y; i < y + height; i++)
		for (unsigned long long j = x; j < x + width; j++)
			*(GOP.framebuffer + (i * GOP.pixelsperscanline) + j) = color;
}

void draw_pixel(unsigned int x, unsigned int y, unsigned long long color)
{
	unsigned int* addr = GOP.framebuffer + (y * GOP.pixelsperscanline) + x;
	*addr = color;
}

static unsigned long long printX = 10;
static unsigned long long printY = 10;

void putc(char ch)
{
	unsigned int font_char_index = ch - ' ';

	for (unsigned long long i = 0; i < 8; i++)
	{
		for (unsigned long long j = 0; j < 10; j++)
		{
			if ((font[(font_char_index * 10) + j] & (1 << (7 - i))) != 0)
				draw_pixel((printX + i), (printY + j), 0xFFFFFFFF);
		}
	}

	printX += 10;
	if (printX >= (GOP.width-8))	// 8 is used as character size
	{
		printX = 10;
		printY += 12;
	}
}

static void print_int(int i)
{
	char _tmp[24];
	itoa(i, _tmp, 10);

	int idx = 0;

	while (_tmp[idx++] != '\0')
	{
		putc(*_tmp);
	}
}

static void print_double(double d)
{
	char* _tmp = itoa(d, (void*)0, 10);

	while (*_tmp != '\0')
	{
		putc(*_tmp);
		_tmp++;
	}
}

static void print_hex(int h)
{
	char _tmp[24];
	itoa(h, _tmp, 16);

	unsigned long long idx = 0;
	
	while (_tmp[idx] != '\0')
	{
		putc(_tmp[idx]);
		idx++;
	}
}

static void print_binary(int h)
{
	char _tmp[24];
	itoa(h, _tmp, 2);

	unsigned long long idx = 0;
	
	while (_tmp[idx] != '\0')
	{
		putc(_tmp[idx]);
		idx++;
	}
}

char buffer[128];
static char* to_string(unsigned long long value)
{
	unsigned long long idx = 0;

	// Get number of digits
	unsigned long long tmp_value  = value;
	unsigned long long digit_count = 0;
	while (tmp_value /= 10)
		digit_count++;

	// Cycle through all digits
	buffer[digit_count+1] = '\0';
	buffer[digit_count - idx++] = (char)(value % 10) + '0';
	while (value /= 10)
		buffer[digit_count - idx++] = (char)(value % 10) + '0';
	
	return buffer;
}

static char* pad(char* destination, const char* source, size_t size, char pad_type)
{
	if (strlen(source) >= size)
	{
		strcpy(destination, source);
		return destination;
	}
	
	memset(destination, pad_type, size);
	destination[size] = 0;

	if (source[0] != '-')
	{
		strcpy((destination + size - strlen(source)), source);
	}
	else
	{
		strcpy((destination + size - strlen(source)) + 1, source);
		destination[size - strlen(source) + 1] = pad_type;
		destination[0] = '-';
	}

	return destination;
}

void printf(const char* fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);

	int padding = 0;
	int idx = 0;

	while (fmt[idx] != '\0') {
		if (fmt[idx] == '\n')
			printY += 12;
		else if (fmt[idx] == '\r')
			printX = 10;
		else if (fmt[idx] == '%') {
			idx++;
			if (fmt[idx] == '%') {
				putc('%');
			}
			else if (fmt[idx] == '0')	// e.g. "%02"
			{
				idx++;
				padding = fmt[idx] - '0';
			}
			else if (fmt[idx] == 'i') {
				print_int(va_arg(argp, int));
			}
			else if (fmt[idx] == 'l') {
				printf(to_string(va_arg(argp, unsigned long long)));
			}
			else if (fmt[idx] == 'd') {
				print_int(va_arg(argp, int));
			}
			else if (fmt[idx] == 'x')
			{
				char integer_buffer[96];
				char padded_buffer[96];

				int hex = va_arg(argp, unsigned int);
				itoa(hex, integer_buffer, 16);

				pad(padded_buffer, integer_buffer, padding, '0');

				for (int i = 0; i < padding; i++)
					printf("%c", padded_buffer[i]);
			}
			else if (fmt[idx] == 's') {
				printf(va_arg(argp, char*));
			}
			else if (fmt[idx] == 'c') {
				putc(va_arg(argp, int));
			}
			else if (fmt[idx] == 'b') {
				print_binary(va_arg(argp, int));
			}
			else {
				printf("Not implemented");
			}
		}
		else {
			putc(fmt[idx]);
		}
		idx++;
	}
	va_end(argp);
}

void krnl_set_graphics_ouutput_protocol(unsigned int* fb, unsigned long long width, unsigned long long height, unsigned long long bpp, unsigned long long pixelsperscanline)
{
	GOP.framebuffer = fb;
	GOP.width = width;
	GOP.height = height;
	GOP.bpp = bpp;
	GOP.pixelsperscanline = pixelsperscanline;
}

unsigned int* krnl_get_framebuffer(void) { return GOP.framebuffer; }
void krnl_printf_reset_x(void) { printX = 0; }
void krnl_printf_reset_y(void) { printY = 0; }

void DEBUG(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	printf(fmt, args);

	va_end(args);
}

void PANIC(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	printf(fmt, args);
	
	va_end(args);

	__asm__ volatile ("cli");
	__asm__ volatile ("hlt");
}