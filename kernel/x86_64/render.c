#include <render.h>
#include <log.h>
#include <workqueue.h>
#include <stdint.h>
#include <string.h>
#include <mm/pgtable.h> // Assumed to be provided
#include <mm/pmm.h>     // Assumed to be provided
#include <gui/font8x8_basic.h> // Font data
#include <kernel.h>


#define CHAR_WIDTH      8
#define CHAR_HEIGHT     8
#define WIDTH           boot_info_v2->fb_pixel_format
#define HEIGHT          boot_info_v2->fb_height
#define FB_SIZE         boot_info_v2->fb_size

#define MAX_SCALE       4
static int text_scale = 1;
static int line_spacing = 2;

// Compute number of columns and rows.
#define COLS            (WIDTH / (CHAR_WIDTH * text_scale))
#define ROWS            (HEIGHT / (CHAR_HEIGHT * text_scale + line_spacing))

uint32_t *framebuf_base;
static uint32_t *framebuf_back = 0;

// Rendering state.
static int render_current_row = 0;
static int render_in_progress = 0;

// Work structure for deferred rendering.
static struct work_struct render_work;
static int render_work_scheduled = 0;

// Forward declarations.
static void draw_scaled_pixel(int sx, int sy, uint32_t color);
static void draw_char_at(char ch, int col, int row);
static void render_row(int row);

// Low–level framebuffer initialization.
// Assumes early_map_entries(), kmalloc(), __memset8(), and kprintf() are defined by your OS.
static void framebuf_static_init(void) {
    kprintf("framebuffer:\n");
    kprintf("  width: %llu\n", WIDTH);
    kprintf("  height: %llu\n", HEIGHT);
    kprintf("  size: %llu\n", FB_SIZE);
    kprintf("  pitch: %llu\n", boot_info_v2->fb_pixel_format);
  render_init();

    framebuf_base = early_map_entries(FRAMEBUFFER_VA, boot_info_v2->fb_addr,
                      FB_SIZE / PAGE_SIZE, VM_RDWR | VM_FIXED);

    framebuf_back = (uint32_t *) kmalloc(FB_SIZE);
    memset((void *)framebuf_base, 0x00, FB_SIZE);
    memset((void *)framebuf_back, 0x00, FB_SIZE);
}

STATIC_INIT(framebuf_static_init);

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
    char *glyph = font8x8_basic[(uint8_t)ch];
    int sx = col * CHAR_WIDTH * text_scale;
    int sy = row * (CHAR_HEIGHT * text_scale + line_spacing);
    for (int i = 0; i < CHAR_HEIGHT; i++) {
        for (int j = 0; j < CHAR_WIDTH; j++) {
            uint32_t color = (glyph[i] & (1 << j)) ? 0xFFFFFFFF : 0x00000000;
            draw_scaled_pixel(sx + j * text_scale, sy + i * text_scale, color);
        }
    }
}

#include <8250.h>

static void render_row(int row) {
    int log_row_count = log_get_row_count();
    // Display the last ROWS log rows (simple scrolling).
    int start_log_row = (log_row_count > ROWS) ? log_row_count - ROWS : 0;
    int log_row = start_log_row + row;
    int sy = row * (CHAR_HEIGHT * text_scale + line_spacing);
    // Clear the row area.
    for (int y = sy; y < sy + CHAR_HEIGHT * text_scale + line_spacing; y++) {
        for (int x = 0; x < WIDTH; x++) {
            framebuf_back[y * WIDTH + x] = 0;
        }
    }
    if (log_row < log_row_count) {
        const char *line = log_get_row(log_row);
        int len = 0;
        while (line[len] && len < COLS) { len++; }
        for (int col = 0; col < len; col++) {
            draw_char_at(line[col], col, row);
        }
    }
}

// The render work function now processes all rows in one go.
static void render_work_func(void *data) {
    (void)data; // unused
    for (int row = 0; row < ROWS; row++) {
         render_row(row);
    }
    memcpy(framebuf_base, framebuf_back, FB_SIZE);
    render_in_progress = 0;
    render_work_scheduled = 0;
}

void render_tick(void) {
    render_work_func(0);
}

void render_init(void) {
    render_work.func = render_work_func;
    render_work.data = 0;
    render_work.next = 0;
    // Allocate the backbuffer with the full framebuffer size.
    framebuf_back = (uint32_t *) kmalloc(FB_SIZE);
    if (framebuf_back)
        memset((void *)framebuf_back, 0x00, FB_SIZE);
    // framebuf_base is assumed to be set up elsewhere (e.g. in a static initializer).
}

void render_deferred(void) {
    if (!render_in_progress) {
        render_in_progress = 1;
        if (!render_work_scheduled) {
            render_work_scheduled = 1;
            schedule_work(&render_work);
        }
    }
}