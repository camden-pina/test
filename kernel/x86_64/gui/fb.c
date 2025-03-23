//
// Enhanced TTY Framebuffer Renderer
// Supports: buffering, auto-scroll, scaling, spacing, word wrapping, flicker-free redraw, and efficient buffered output
// Created by Aaron Gill-Braun on 2021-04-20.
// Enhanced by ChatGPT
//

/*
#include <gui/font8x8_basic.h>
#include <mm/pmm.h>
#include <string.h>
#include <printf.h>
#include <mm/pgtable.h>

#define CHAR_WIDTH      8
#define CHAR_HEIGHT     8

#define WIDTH           boot_info_v2->fb_width
#define HEIGHT          boot_info_v2->fb_height
#define FB_SIZE         boot_info_v2->fb_size

#define MAX_SCALE       4
#define TTY_MAX_ROWS    1024
#define OUTPUT_QUEUE_SIZE 256
#define COLS            (WIDTH / (CHAR_WIDTH * text_scale))
#define ROWS            (HEIGHT / (CHAR_HEIGHT * text_scale + line_spacing))

static int cursor_x = 0;
static int cursor_y = 0;

static int text_scale = 2;
static int line_spacing = 2;

static char tty_buffer[TTY_MAX_ROWS][256];
static int tty_row_count = 0;
static int tty_row_start = 0;

static char output_queue[OUTPUT_QUEUE_SIZE];
static int output_queue_len = 0;

uint32_t *framebuf_base;
static uint32_t *framebuf_back = NULL;

// --- Framebuffer Init ---

static void framebuf_static_init() {
  kprintf("framebuffer:\n");
  kprintf("  width: %llu\n", WIDTH);
  kprintf("  height: %llu\n", HEIGHT);
  kprintf("  size: %llu\n", FB_SIZE);

  early_map_entries(FRAMEBUFFER_VA, boot_info_v2->fb_addr, FB_SIZE / PAGE_SIZE, VM_RDWR | VM_FIXED);
  framebuf_base = (uint32_t *) FRAMEBUFFER_VA;

  framebuf_back = (uint32_t *) kmalloc(FB_SIZE / PAGE_SIZE);
  __memset8((void *)framebuf_back, 0x00, FB_SIZE);
}
STATIC_INIT(framebuf_static_init);

// --- Drawing Primitives ---

static void draw_scaled_pixel(int sx, int sy, uint32_t color) {
  for (int dy = 0; dy < text_scale; dy++) {
    for (int dx = 0; dx < text_scale; dx++) {
      int px = sx + dx;
      int py = sy + dy;
      if (px < WIDTH && py < HEIGHT)
        framebuf_back[py * WIDTH + px] = color;
    }
  }
}

static void draw_char_at(char ch, int col, int row) {
  if (col >= COLS || row >= ROWS)
    return;

  char *glyph = font8x8_basic[(uint8_t) ch];
  int sx = col * CHAR_WIDTH * text_scale;
  int sy = row * (CHAR_HEIGHT * text_scale + line_spacing);

  for (int i = 0; i < CHAR_HEIGHT; i++) {
    for (int j = 0; j < CHAR_WIDTH; j++) {
      uint32_t color = (glyph[i] & (1 << j)) ? UINT32_MAX : 0x00000000;
      draw_scaled_pixel(sx + j * text_scale, sy + i * text_scale, color);
    }
  }
}

// --- Screen Control ---

static void screen_clear() {
  __memset8((void *) framebuf_back, 0x00, FB_SIZE);
  __memset8((void *) framebuf_base, 0x00, FB_SIZE);
  cursor_x = 0;
  cursor_y = 0;
}

static void screen_scroll_up() {
  tty_row_start = (tty_row_start + 1) % TTY_MAX_ROWS;
  int clear_row = (tty_row_start + ROWS - 1) % TTY_MAX_ROWS;
  memset(tty_buffer[clear_row], 0, COLS);
  cursor_y = ROWS - 1;
}

static void screen_render_from_buffer() {
  __memset8((void *) framebuf_back, 0x00, FB_SIZE);
  for (int row = 0; row < ROWS; row++) {
    int buffer_row = (tty_row_start + row) % TTY_MAX_ROWS;
    for (int col = 0; col < COLS; col++) {
      char ch = tty_buffer[buffer_row][col];
      draw_char_at(ch ? ch : ' ', col, row);
    }
  }
  memcpy(framebuf_base, framebuf_back, FB_SIZE);
}

// --- Character Printing (Buffered) ---

static void screen_print_buffered_char(char ch) {
  if (ch == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if (ch == '\r') {
    cursor_x = 0;
  } else if (ch == '\t') {
    cursor_x += 4;
  } else if (ch == '\b') {
    if (cursor_x > 0)
      cursor_x--;
    tty_buffer[(tty_row_start + cursor_y) % TTY_MAX_ROWS][cursor_x] = ' ';
  } else {
    if (cursor_x >= COLS) {
      cursor_x = 0;
      cursor_y++;
    }

    if (cursor_y >= ROWS) {
      screen_scroll_up();
    }

    int row_idx = (tty_row_start + cursor_y) % TTY_MAX_ROWS;
    tty_buffer[row_idx][cursor_x] = ch;
    cursor_x++;
  }
}

void screen_flush() {
  for (int i = 0; i < output_queue_len; i++) {
    screen_print_buffered_char(output_queue[i]);
  }
  output_queue_len = 0;
  screen_render_from_buffer();
}

void screen_print_char(char ch) {
  if (output_queue_len < OUTPUT_QUEUE_SIZE) {
    output_queue[output_queue_len++] = ch;
  }

  if (ch == '\n' || output_queue_len >= OUTPUT_QUEUE_SIZE) {
    screen_flush();
  }
}

// --- Word & String Printing ---

static void screen_print_word(const char *word, int len) {
  if (cursor_x + len > COLS) {
    cursor_x = 0;
    cursor_y++;
  }
  for (int i = 0; i < len; i++) {
    screen_print_char(word[i]);
  }
}

void screen_print_str(const char *str) {
  const char *start = str;
  while (*start) {
    const char *end = start;
    while (*end && *end != ' ') end++;
    int len = end - start;
    if (len > 0)
      screen_print_word(start, len);
    if (*end == ' ') {
      screen_print_char(' ');
      end++;
    }
    start = end;
  }
  screen_flush();
}

// --- Configuration API ---

void screen_set_scale(int scale) {
  if (scale < 1 || scale > MAX_SCALE) return;
  text_scale = scale;
  screen_render_from_buffer();
}

void screen_set_spacing(int spacing) {
  line_spacing = spacing;
  screen_render_from_buffer();
}

// --- Startup ---

static void screen_init() {
  screen_clear();
  tty_row_count = 0;
  tty_row_start = 0;
  output_queue_len = 0;
}
STATIC_INIT(screen_init);

*/