// render.c
#include <render.h>
#include <log.h>
#include <workqueue.h>
#include <stdint.h>
#include <string.h>
#include <mm/pgtable.h> // Assumed to be provided
#include <mm/pmm.h>     // Assumed to be provided
#include <gui/font8x8_basic.h> // Font data
#include <kernel.h>

#define CHAR_WIDTH       8
#define CHAR_HEIGHT      8

// These come from your boot_info_v2 or wherever you store framebuffer info
#define WIDTH            boot_info_v2->fb_width
#define HEIGHT           boot_info_v2->fb_height
#define FB_SIZE          boot_info_v2->fb_size

// Font scaling
#define MAX_SCALE        4
static int text_scale   = 1;
static int line_spacing = 2;

// Compute columns and rows for text
#define COLS (WIDTH / (CHAR_WIDTH * text_scale))
#define ROWS (HEIGHT / (CHAR_HEIGHT * text_scale + line_spacing))

// Pointer to the actual VRAM (framebuffer)—we write here, but do NOT read
uint32_t *framebuf_base = NULL;

// Maintain two RAM buffers. No reads from VRAM.
static uint32_t *framebuf_back = NULL; // “New content” buffer
static uint32_t *framebuf_prev = NULL; // “Previous content” buffer

// Render state
static int render_in_progress     = 0;
static int render_work_scheduled  = 0;
static struct work_struct render_work;

// Forward declarations
static void draw_scaled_pixel(int sx, int sy, uint32_t color);
static void draw_char_at(char ch, int col, int row);
static void render_row(int row);
static void sync_buffers(void);

//-----------------------------------------------------------------------------
// Framebuffer (VRAM) static initialization
//-----------------------------------------------------------------------------
static void framebuf_static_init(void) {
    kprintf("framebuffer:\n");
    kprintf("  width: %llu\n", WIDTH);
    kprintf("  height: %llu\n", HEIGHT);
    kprintf("  size: %llu\n", FB_SIZE);
    kprintf("  pitch: %llu\n", boot_info_v2->fb_pixel_format);

    // Call render_init to set up local buffers
    render_init();

    // Map the actual framebuffer (VRAM) into kernel space
    framebuf_base = early_map_entries(FRAMEBUFFER_VA,
                                      boot_info_v2->fb_addr,
                                      FB_SIZE / PAGE_SIZE,
                                      VM_RDWR | VM_FIXED);

    // Clear VRAM once so we start with a blank display (write-only!)
    memset((void *)framebuf_base, 0x00, FB_SIZE);
}

// The macro or function that runs this at startup
STATIC_INIT(framebuf_static_init);

//-----------------------------------------------------------------------------
// Draw scaled pixels into the "back" buffer in RAM
// (No VRAM reads here—entirely in system memory.)
//-----------------------------------------------------------------------------
static void draw_scaled_pixel(int sx, int sy, uint32_t color) {
    for (int dy = 0; dy < text_scale; dy++) {
        for (int dx = 0; dx < text_scale; dx++) {
            int px = sx + dx;
            int py = sy + dy;
            if (px < WIDTH && py < HEIGHT) {
                framebuf_back[py * WIDTH + px] = color;
            }
        }
    }
}

static void draw_char_at(char ch, int col, int row) {
    if (col >= COLS || row >= ROWS) {
        return;
    }
    // Retrieve the glyph from the 8x8 font table
    uint8_t *glyph = (uint8_t *)font8x8_basic[(uint8_t)ch];

    int sx = col * CHAR_WIDTH * text_scale;
    int sy = row * (CHAR_HEIGHT * text_scale + line_spacing);

    for (int i = 0; i < CHAR_HEIGHT; i++) {
        for (int j = 0; j < CHAR_WIDTH; j++) {
            // Check if this pixel in the glyph is set
            uint32_t color = (glyph[i] & (1 << j)) ? 0xFFFFFFFF : 0x00000000;
            draw_scaled_pixel(sx + j * text_scale, sy + i * text_scale, color);
        }
    }
}

#include <8250.h>

// Renders a single row (into backbuffer) by pulling from log buffer
static void render_row(int row) {
    int log_row_count = log_get_row_count();

    // Display the last ROWS lines from the log
    int start_log_row = (log_row_count > ROWS) ? (log_row_count - ROWS) : 0;
    int log_row = start_log_row + row;

    // Y coordinate in pixel space
    int sy = row * (CHAR_HEIGHT * text_scale + line_spacing);

    // Clear that row in the backbuffer
    for (int y = sy; y < sy + (CHAR_HEIGHT * text_scale + line_spacing); y++) {
        for (int x = 0; x < WIDTH; x++) {
            framebuf_back[y * WIDTH + x] = 0;
        }
    }

    // Draw text if available
    if (log_row < log_row_count) {
        const char *line = log_get_row(log_row);
        int len = 0;
        while (line[len] && len < COLS) {
            len++;
        }
        for (int col = 0; col < len; col++) {
            draw_char_at(line[col], col, row);
        }
    }
}

//-----------------------------------------------------------------------------
// Compare the “back” buffer to the “previous” buffer (both in RAM)
// and write only changed pixels to VRAM.
//-----------------------------------------------------------------------------
static void sync_buffers(void) {
    size_t pixel_count = FB_SIZE / sizeof(uint32_t);

    for (size_t i = 0; i < pixel_count; i++) {
        uint32_t new_val = framebuf_back[i];
        if (new_val != framebuf_prev[i]) {
            // Pixel changed -> write to VRAM (no read from VRAM!) + update prev
            framebuf_base[i] = new_val;
            framebuf_prev[i] = new_val;
        }
    }
}

//-----------------------------------------------------------------------------
// The “work” function that re-renders everything and syncs
//-----------------------------------------------------------------------------
static void render_work_func(void *data) {
    (void)data; // Unused

    // Redraw every row from the log
    for (int row = 0; row < ROWS; row++) {
        render_row(row);
    }

    // Now push only the changed pixels out to VRAM
    sync_buffers();

    render_in_progress     = 0;
    render_work_scheduled  = 0;
}

//-----------------------------------------------------------------------------
// Functions to call for either immediate or deferred render
//-----------------------------------------------------------------------------
void render_tick(void) {
    // If you want a quick way to refresh from e.g. a timer callback,
    // you can call the worker function directly:
    render_work_func(NULL);
}

void render_init(void) {
    // Prepare the work item
    render_work.func = render_work_func;
    render_work.data = 0;
    render_work.next = 0;

    // Allocate backbuffer once
    if (!framebuf_back) {
        framebuf_back = (uint32_t *)kmalloc(FB_SIZE);
        if (framebuf_back) {
            memset(framebuf_back, 0x00, FB_SIZE);
        }
    }

    // Allocate “previous” buffer
    if (!framebuf_prev) {
        framebuf_prev = (uint32_t *)kmalloc(FB_SIZE);
        if (framebuf_prev) {
            memset(framebuf_prev, 0x00, FB_SIZE);
        }
    }
}

void render_deferred(void) {
    // Schedules a render in the kernel workqueue
    if (!render_in_progress) {
        render_in_progress = 1;
        if (!render_work_scheduled) {
            render_work_scheduled = 1;
            schedule_work(&render_work);
        }
    }
}
