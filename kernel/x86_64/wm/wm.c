/* compositor_final.c - A robust, efficient, ghost-free compositor (Optimized) */

#include <kernel.h>
#include <mm/pmm.h>
#include <printf.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <wm/wallpaper.h>
#include <emmintrin.h>  // Added for SIMD intrinsics

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------
#define MAX_WINDOWS        32
#define MAX_DAMAGE_RECTS   256    // raised from 64 to 256
#define EVENT_QUEUE_SIZE   128
#define MAX_PLUGINS        16

// If your OS doesn't have a real timer yet, this is a placeholder:
static uint64_t get_millis(void) {
    static uint64_t fake_time = 0;
    fake_time += 16;  // ~60 FPS
    return fake_time;
}

static bool g_blur_enabled = true;

// -----------------------------------------------------------------------------
// Basic data structures
// -----------------------------------------------------------------------------
typedef enum {
    EVENT_KEY,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_BUTTON
} EventType;

typedef struct {
    EventType type;
    union {
        struct {
            uint8_t scancode;
            char ch;
            bool pressed;
        } key;
        struct {
            int x, y;
            int dx, dy;
            uint8_t buttons;
        } mouse_move;
        struct {
            int x, y;
            uint8_t button;
            bool pressed;
        } mouse_button;
    };
} Event;

typedef struct {
    int x, y, w, h;
} DamageRect;

struct Window;  // forward declaration

typedef struct Window {
    int x, y;
    int width, height;
    uint32_t *buffer;    // pixel buffer (ARGB or RGBA)
    char *title;
    bool has_shadow;
    bool blur_background;
    bool transparent;
    uint8_t opacity;        // current alpha
    uint8_t target_opacity; // future alpha (if we re-enable fade)
} Window;

typedef struct {
    char name[32];
    int (*init)(void);
    int (*on_event)(Event *);
    int (*on_frame)(void);
    bool enabled;
} Plugin;

static struct {
    uint32_t *fb_addr;
    uint32_t *backbuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t pixel_format;
} fb;

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
static Window *windows[MAX_WINDOWS];
static int window_count = 0;
static Window *focused_window = NULL;
static Window *dragging_window = NULL;
static int drag_offset_x = 0, drag_offset_y = 0;

static Event event_queue[EVENT_QUEUE_SIZE];
static int event_head = 0;
static int event_tail = 0;
static volatile int event_lock = 0;  // spinlock

static Plugin plugins[MAX_PLUGINS];
static int plugin_count = 0;

static DamageRect damage_rects[MAX_DAMAGE_RECTS];
static int damage_count = 0;

static int mouse_x = 0;
static int mouse_y = 0;
static uint8_t mouse_buttons = 0;

static unsigned char keycode_map[128] = {
    /* fill in or keep from your code */
};

static unsigned char font8x16[96][16] = {
    /* fill in or keep from your code */
};

// -----------------------------------------------------------------------------
// Spinlock for event queue
// -----------------------------------------------------------------------------
static inline void lock_event_queue(void) {
    while (__sync_lock_test_and_set(&event_lock, 1)) { /* spin */ }
}
static inline void unlock_event_queue(void) {
    __sync_lock_release(&event_lock);
}

// -----------------------------------------------------------------------------
// Event queue
// -----------------------------------------------------------------------------
static bool enqueue_event(const Event *ev) {
    lock_event_queue();
    int next_tail = (event_tail + 1) % EVENT_QUEUE_SIZE;
    if (next_tail == event_head) {
        kprintf("[WM] Event queue overflow, dropping event!\n");
        unlock_event_queue();
        return false;
    }
    event_queue[event_tail] = *ev;
    event_tail = next_tail;
    unlock_event_queue();
    return true;
}

static bool dequeue_event(Event *ev) {
    lock_event_queue();
    if (event_head == event_tail) {
        unlock_event_queue();
        return false;
    }
    *ev = event_queue[event_head];
    event_head = (event_head + 1) % EVENT_QUEUE_SIZE;
    unlock_event_queue();
    return true;
}

// -----------------------------------------------------------------------------
// Damage tracking
// -----------------------------------------------------------------------------
static void add_damage_rect(int x, int y, int w, int h) {
    w += 5;
    h += 5;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb.width)  w = fb.width - x;
    if (y + h > (int)fb.height) h = fb.height - y;
    if (w <= 0 || h <= 0) return;

    if (damage_count >= MAX_DAMAGE_RECTS) {
        kprintf("[WM] Damage array full, fallback to full screen.\n");
        damage_count = 0;
        damage_rects[0].x = 0;
        damage_rects[0].y = 0;
        damage_rects[0].w = fb.width;
        damage_rects[0].h = fb.height;
        damage_count = 1;
        return;
    }
    damage_rects[damage_count].x = x;
    damage_rects[damage_count].y = y;
    damage_rects[damage_count].w = w;
    damage_rects[damage_count].h = h;
    damage_count++;
}

static bool get_merged_damage(int *x, int *y, int *w, int *h) {
    if (damage_count == 0) return false;
    int minx = 999999, miny = 999999;
    int maxx = -1, maxy = -1;
    for (int i = 0; i < damage_count; i++) {
        DamageRect *dr = &damage_rects[i];
        if (dr->x < minx) minx = dr->x;
        if (dr->y < miny) miny = dr->y;
        if (dr->x + dr->w > maxx) maxx = dr->x + dr->w;
        if (dr->y + dr->h > maxy) maxy = dr->y + dr->h;
    }
    *x = minx;
    *y = miny;
    *w = maxx - minx;
    *h = maxy - miny;
    return true;
}

static void clear_damage(void) {
    damage_count = 0;
}

// -----------------------------------------------------------------------------
// find_topmost_window_under_cursor
// -----------------------------------------------------------------------------
static Window* find_topmost_window_under_cursor(int x, int y)
{
    Window* top = NULL;
    for (int i = 0; i < window_count; i++) {
        Window* w = windows[i];
        if (x >= w->x && x < w->x + w->width &&
            y >= w->y && y < w->y + w->height) {
            top = w;
        }
    }
    return top;
}

// -----------------------------------------------------------------------------
// External handlers for PS/2 IRQ or xHCI
// -----------------------------------------------------------------------------
void wm_handle_mouse(int8_t dx, int8_t dy, uint8_t status) {
    int old_x = mouse_x;
    int old_y = mouse_y;
    mouse_x += dx;
    mouse_y -= dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= (int)fb.width)  mouse_x = fb.width - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= (int)fb.height) mouse_y = fb.height - 1;

    uint8_t new_buttons = status & 0x07;
    int minx = (old_x < mouse_x) ? old_x : mouse_x;
    int maxx = (old_x > mouse_x) ? old_x : mouse_x;
    int miny = (old_y < mouse_y) ? old_y : mouse_y;
    int maxy = (old_y > mouse_y) ? old_y : mouse_y;
    minx -= 4; miny -= 4;
    maxx += 4; maxy += 4;
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= (int)fb.width)  maxx = fb.width - 1;
    if (maxy >= (int)fb.height) maxy = fb.height - 1;
    add_damage_rect(minx, miny, (maxx - minx + 1), (maxy - miny + 1));

    if (dx != 0 || dy != 0) {
        Event mev;
        mev.type = EVENT_MOUSE_MOVE;
        mev.mouse_move.x = mouse_x;
        mev.mouse_move.y = mouse_y;
        mev.mouse_move.dx = dx;
        mev.mouse_move.dy = dy;
        mev.mouse_move.buttons = new_buttons;
        enqueue_event(&mev);
    }

    for (uint8_t i = 0; i < 3; i++) {
        uint8_t mask = (1 << i);
        bool was_pressed = (mouse_buttons & mask) != 0;
        bool now_pressed = (new_buttons & mask) != 0;
        if (now_pressed && !was_pressed) {
            Event bev;
            bev.type = EVENT_MOUSE_BUTTON;
            bev.mouse_button.x = mouse_x;
            bev.mouse_button.y = mouse_y;
            bev.mouse_button.button = i;
            bev.mouse_button.pressed = true;
            enqueue_event(&bev);
        } else if (!now_pressed && was_pressed) {
            Event bev;
            bev.type = EVENT_MOUSE_BUTTON;
            bev.mouse_button.x = mouse_x;
            bev.mouse_button.y = mouse_y;
            bev.mouse_button.button = i;
            bev.mouse_button.pressed = false;
            enqueue_event(&bev);
        }
    }
    mouse_buttons = new_buttons;
}

void wm_handle_keyboard_scancode(uint8_t scancode) {
    Event ev;
    ev.type = EVENT_KEY;
    ev.key.pressed = !(scancode & 0x80);
    uint8_t code = scancode & 0x7F;
    ev.key.scancode = code;
    char c = (code < 128) ? keycode_map[code] : 0;
    ev.key.ch = (c >= 0x20 && c < 0x7F) ? c : 0;
    enqueue_event(&ev);
}

// -----------------------------------------------------------------------------
// Basic drawing / alpha blending
// -----------------------------------------------------------------------------
static inline void put_pixel(int x, int y, uint32_t color) {
    if ((unsigned)x >= fb.width || (unsigned)y >= fb.height) return;
    fb.backbuffer[y * fb.width + x] = color;
}

static inline uint32_t blend_pixel(uint32_t src, uint32_t dst) {
    uint8_t alpha = (src >> 24) & 0xFF;
    if (alpha == 0) return dst;
    if (alpha == 0xFF) return src;
    uint8_t sr = (src >> 16) & 0xFF;
    uint8_t sg = (src >> 8) & 0xFF;
    uint8_t sb = (src) & 0xFF;
    uint8_t dr = (dst >> 16) & 0xFF;
    uint8_t dg = (dst >> 8) & 0xFF;
    uint8_t db = (dst) & 0xFF;
    uint8_t out_r = (uint8_t)((sr * alpha + dr * (255 - alpha)) / 255);
    uint8_t out_g = (uint8_t)((sg * alpha + dg * (255 - alpha)) / 255);
    uint8_t out_b = (uint8_t)((sb * alpha + db * (255 - alpha)) / 255);
    return (0xFF << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

// -----------------------------------------------------------------------------
// Optimized fill_rect using SIMD (SSE2) for accelerated drawing loops
// -----------------------------------------------------------------------------
static inline void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb.width)  w = fb.width - x;
    if (y + h > (int)fb.height) h = fb.height - y;
    if (w <= 0 || h <= 0) return;

    __m128i fill = _mm_set1_epi32(color);
    for (int yy = 0; yy < h; yy++) {
        uint32_t *row = fb.backbuffer + (y + yy) * fb.width + x;
        int xx = 0;
        for (; xx <= w - 4; xx += 4) {
            _mm_storeu_si128((__m128i*)(row + xx), fill);
        }
        for (; xx < w; xx++) {
            row[xx] = color;
        }
    }
}

// -----------------------------------------------------------------------------
// Optimized blur_region using separable box blur
// -----------------------------------------------------------------------------
static void blur_region(int x, int y, int w, int h) {
    if (!g_blur_enabled) return;
    if (x < 1) { w += x; x = 1; }
    if (y < 1) { h += y; y = 1; }
    if (x + w > (int)fb.width - 1)  w = (fb.width - 1) - x;
    if (y + h > (int)fb.height - 1) h = (fb.height - 1) - y;
    if (w <= 1 || h <= 1) return;

    uint32_t *temp = kmalloc(w * h * sizeof(uint32_t));
    if (!temp) {
        kprintf("[WM] blur_region: allocation failed\n");
        return;
    }

    // Horizontal pass: average each pixel with its left and right neighbors.
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int sum_a = 0, sum_r = 0, sum_g = 0, sum_b = 0;
            int count = 0;
            for (int k = -1; k <= 1; k++) {
                int c = col + k;
                if (c < 0 || c >= w) continue;
                uint32_t pixel = fb.backbuffer[(y + row) * fb.width + (x + c)];
                sum_a += (pixel >> 24) & 0xFF;
                sum_r += (pixel >> 16) & 0xFF;
                sum_g += (pixel >> 8) & 0xFF;
                sum_b += pixel & 0xFF;
                count++;
            }
            uint8_t a = sum_a / count;
            uint8_t r = sum_r / count;
            uint8_t g = sum_g / count;
            uint8_t b = sum_b / count;
            temp[row * w + col] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    // Vertical pass: average each pixel with its top and bottom neighbors from the temp buffer.
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int sum_a = 0, sum_r = 0, sum_g = 0, sum_b = 0;
            int count = 0;
            for (int k = -1; k <= 1; k++) {
                int r_index = row + k;
                if (r_index < 0 || r_index >= h) continue;
                uint32_t pixel = temp[r_index * w + col];
                sum_a += (pixel >> 24) & 0xFF;
                sum_r += (pixel >> 16) & 0xFF;
                sum_g += (pixel >> 8) & 0xFF;
                sum_b += pixel & 0xFF;
                count++;
            }
            uint8_t a = sum_a / count;
            uint8_t r = sum_r / count;
            uint8_t g = sum_g / count;
            uint8_t b = sum_b / count;
            fb.backbuffer[(y + row) * fb.width + (x + col)] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    kfree(temp);
}

// Helper: Linear blend between two ARGB colors (t in [0,1])
static inline uint32_t blend_color(uint32_t c1, uint32_t c2, float t) {
    uint8_t a1 = (c1 >> 24) & 0xFF, r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint8_t a2 = (c2 >> 24) & 0xFF, r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    uint8_t a = (uint8_t)((1 - t) * a1 + t * a2);
    uint8_t r = (uint8_t)((1 - t) * r1 + t * r2);
    uint8_t g = (uint8_t)((1 - t) * g1 + t * g2);
    uint8_t b = (uint8_t)((1 - t) * b1 + t * b2);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Helper: Apply rounded corners to a window’s buffer (radius in pixels)
static void apply_rounded_corners(Window *win, int radius) {
    if (radius <= 0) return;
    int w = win->width, h = win->height;
    for (int y = 0; y < radius; y++) {
        for (int x = 0; x < radius; x++) {
            int dx = radius - x - 1;
            int dy = radius - y - 1;
            if ((dx * dx + dy * dy) > (radius * radius)) {
                win->buffer[y * w + x] &= 0x00FFFFFF;
            }
        }
    }
    for (int y = 0; y < radius; y++) {
        for (int x = w - radius; x < w; x++) {
            int dx = x - (w - radius);
            int dy = radius - y - 1;
            if ((dx * dx + dy * dy) > (radius * radius)) {
                win->buffer[y * w + x] &= 0x00FFFFFF;
            }
        }
    }
    for (int y = h - radius; y < h; y++) {
        for (int x = 0; x < radius; x++) {
            int dx = radius - x - 1;
            int dy = y - (h - radius);
            if ((dx * dx + dy * dy) > (radius * radius)) {
                win->buffer[y * w + x] &= 0x00FFFFFF;
            }
        }
    }
    for (int y = h - radius; y < h; y++) {
        for (int x = w - radius; x < w; x++) {
            int dx = x - (w - radius);
            int dy = y - (h - radius);
            if ((dx * dx + dy * dy) > (radius * radius)) {
                win->buffer[y * w + x] &= 0x00FFFFFF;
            }
        }
    }
}

// -------------------- Updated create_window --------------------
static Window* create_window(int x, int y, int w, int h,
                             const char *title,
                             bool has_shadow,
                             bool transparent,
                             bool blur_bg) {
    if (window_count >= MAX_WINDOWS) {
        kprintf("[WM] Cannot create window: max limit %d reached\n", MAX_WINDOWS);
        return NULL;
    }
    Window *win = kmalloc(sizeof(Window));
    if (!win) {
        kprintf("[WM] create_window: out of memory\n");
        return NULL;
    }
    memset(win, 0, sizeof(Window));
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->has_shadow = has_shadow;
    win->transparent = transparent;
    win->blur_background = blur_bg;
    win->opacity = 255;
    win->target_opacity = 255;
    if (title) {
        win->title = (char*)title;
    }
    size_t sz = w * h * sizeof(uint32_t);
    win->buffer = kmalloc(sz);
    if (!win->buffer) {
        kprintf("[WM] create_window: out of memory for buffer\n");
        kfree(win);
        return NULL;
    }
    memset(win->buffer, 0, sz);
    if (!transparent) {
        uint32_t title_start = 0xFF007AFF;
        uint32_t title_end   = 0xFF0051D4;
        int bar_h = (h < 22) ? h : 22;
        for (int yy = 0; yy < bar_h; yy++) {
            float t = (float)yy / (bar_h - 1);
            uint32_t grad_color = blend_color(title_start, title_end, t);
            for (int xx = 0; xx < w; xx++) {
                win->buffer[yy * w + xx] = grad_color;
            }
        }
        uint32_t border_color = 0x20000000;
        for (int xx = 0; xx < w; xx++) {
            win->buffer[(bar_h - 1) * w + xx] = border_color;
        }
        uint32_t body_col = 0xFFF0F0F0;
        for (int yy = bar_h; yy < h; yy++) {
            for (int xx = 0; xx < w; xx++) {
                win->buffer[yy * w + xx] = body_col;
            }
        }
        for (int i = 0; i < (int)(w * h); i++) {
            win->buffer[i] |= 0xFF000000;
        }
        apply_rounded_corners(win, 8);
    } else {
        for (int i = 0; i < (int)(w * h); i++) {
            win->buffer[i] = 0x80FFFFFF;
        }
    }
    windows[window_count++] = win;
    focused_window = win;
    add_damage_rect(x, y, w, h);
    return win;
}

static void focus_window(Window *win) {
    if (!win) return;
    if (windows[window_count - 1] != win) {
        int idx = -1;
        for (int i = 0; i < window_count; i++) {
            if (windows[i] == win) {
                idx = i;
                break;
            }
        }
        if (idx < 0) return;
        for (int i = idx; i < window_count - 1; i++) {
            windows[i] = windows[i + 1];
        }
        windows[window_count - 1] = win;
    }
    if (focused_window && focused_window != win) {
        add_damage_rect(focused_window->x, focused_window->y,
                        focused_window->width, focused_window->height);
    }
    add_damage_rect(win->x, win->y, win->width, win->height);
    focused_window = win;
}

// -----------------------------------------------------------------------------
// Plugins
// -----------------------------------------------------------------------------
int register_plugin(Plugin *p) {
    if (plugin_count >= MAX_PLUGINS) {
        kprintf("[WM] Cannot register plugin %s: max plugin limit.\n", p->name);
        return -1;
    }
    plugins[plugin_count] = *p;
    plugins[plugin_count].enabled = true;
    if (plugins[plugin_count].init) {
        int res = plugins[plugin_count].init();
        if (res < 0) {
            kprintf("[WM] plugin %s init failed (%d). disabling\n", p->name, res);
            plugins[plugin_count].enabled = false;
        }
    }
    kprintf("[WM] Plugin registered: %s (enabled=%d)\n", p->name, plugins[plugin_count].enabled);
    plugin_count++;
    return 0;
}

static int test_plugin_init(void) {
    kprintf("[TestPlugin] init.\n");
    return 0;
}
static int test_plugin_on_event(Event *e) {
    if (e->type == EVENT_KEY && e->key.pressed && e->key.ch == 'P') {
        kprintf("[TestPlugin] 'P' key pressed!\n");
    }
    return 0;
}

#include <panic.h>
#include <mm/vmem.h>

// -----------------------------------------------------------------------------
// Framebuffer initialization
// -----------------------------------------------------------------------------
static bool init_framebuffer(void) {
    if (!boot_info_v2) {
        kprintf("[WM] no boot_info_v2!\n");
        return false;
    }
    fb.width        = boot_info_v2->fb_width;
    fb.height       = boot_info_v2->fb_height;
    fb.pixel_format = boot_info_v2->fb_pixel_format;
    fb.fb_addr      = (uint32_t*)(uintptr_t)boot_info_v2->fb_addr;
    fb.pitch        = fb.width * 4;
    size_t bb_size = boot_info_v2->fb_size;
    print_kheap_info();
    fb.backbuffer = kmalloc(bb_size);
    uint16_t *test = kmalloc(5);
    if (!fb.backbuffer) {
        panic("[WM] failed to allocate backbuffer!\n");
        return false;
    }
    memset(fb.backbuffer, 0, bb_size);
    kprintf("[WM] Framebuffer: %ux%u, format=%u\n", fb.width, fb.height, fb.pixel_format);
    return true;
}

// -----------------------------------------------------------------------------
// Main Compositor
// -----------------------------------------------------------------------------
void wm_init(void) {
    if (!init_framebuffer()) {
        kprintf("[WM] Framebuffer init fail.\n");
        return;
    }
    if (!wallpaper_init()) {
        kprintf("[WM] Wallpaper initialization failed.\n");
    }
    if (!wallpaper_set_from_file("/wp.bmp", WALLPAPER_MODE_TILE)) {
        kprintf("[WM] Failed to load wallpaper.\n");
    }
    mouse_x = fb.width / 2;
    mouse_y = fb.height / 2;
    mouse_buttons = 0;
    damage_count = 0;
    create_window(50, 50, 1120, 1050, "Test Window 1", true, false, false);
    create_window(180, 120, 320, 250, "Transparent Panel", true, true, true);
    Plugin testp = {"TestPlugin", test_plugin_init, test_plugin_on_event, NULL, false};
    register_plugin(&testp);
    kprintf("[WM] init complete.\n");
}

void wm_run(void) {
    kprintf("[WM] entering main loop.\n");
    const uint64_t frame_delay_ms = 16; // ~60fps
    uint64_t last_frame_time = 0;

    wallpaper_draw(fb.backbuffer, fb.width, fb.height);
    memcpy(fb.fb_addr, fb.backbuffer, fb.width * fb.height * sizeof(uint32_t));
    while (true) {
        bool events_processed = false;
        Event ev;
        while (dequeue_event(&ev)) {
            events_processed = true;
            for (int i = 0; i < plugin_count; i++) {
                if (plugins[i].enabled && plugins[i].on_event) {
                    int ret = plugins[i].on_event(&ev);
                    if (ret < 0) {
                        kprintf("[WM] plugin %s disabled (on_event error)\n", plugins[i].name);
                        plugins[i].enabled = false;
                    }
                }
            }
            switch (ev.type) {
            case EVENT_KEY:
                kprintf("[WM] Key %s: sc=%02x ch=%c\n",
                        ev.key.pressed ? "down":"up", ev.key.scancode,
                        ev.key.ch ? ev.key.ch : '?');
                if (ev.key.pressed && (ev.key.ch == 'B' || ev.key.ch == 'b')) {
                    g_blur_enabled = !g_blur_enabled;
                    kprintf("[WM] Blur toggled => %d\n", g_blur_enabled);
                    add_damage_rect(0, 0, fb.width, fb.height);
                }
                break;
            case EVENT_MOUSE_MOVE:
            if (dragging_window && (mouse_buttons & 0x1)) {
                Window *w = dragging_window;
                int oldx = w->x, oldy = w->y;
                int new_x = ev.mouse_move.x - drag_offset_x;
                int new_y = ev.mouse_move.y - drag_offset_y;
                // Clamp new position to screen bounds.
                if (new_x < 0) new_x = 0;
                if (new_y < 0) new_y = 0;
                if (new_x + w->width > (int)fb.width) new_x = fb.width - w->width;
                if (new_y + w->height > (int)fb.height) new_y = fb.height - w->height;
                
                // Compute union of old and new positions.
                int union_x = (oldx < new_x) ? oldx : new_x;
                int union_y = (oldy < new_y) ? oldy : new_y;
                int union_right = ((oldx + w->width) > (new_x + w->width)) ? (oldx + w->width) : (new_x + w->width);
                int union_bottom = ((oldy + w->height) > (new_y + w->height)) ? (oldy + w->height) : (new_y + w->height);
                int union_w = union_right - union_x;
                int union_h = union_bottom - union_y;
                
                // Update window position.
                w->x = new_x;
                w->y = new_y;
                
                add_damage_rect(union_x, union_y, union_w, union_h);
            }
/*
                if (dragging_window && (mouse_buttons & 0x1)) {
                    Window *w = dragging_window;
                    int oldx = w->x, oldy = w->y;
                    w->x = ev.mouse_move.x - drag_offset_x;
                    w->y = ev.mouse_move.y - drag_offset_y;
                    if (w->x < 0) w->x = 0;
                    if (w->y < 0) w->y = 0;
                    if (w->x + w->width > (int)fb.width) {
                        w->x = fb.width - w->width;
                    }
                    if (w->y + w->height > (int)fb.height) {
                        w->y = fb.height - w->height;
                    }
                    add_damage_rect(oldx, oldy, w->width, w->height);
                    add_damage_rect(w->x, w->y, w->width, w->height);
                }
                    */
                break;
            case EVENT_MOUSE_BUTTON:
                if (ev.mouse_button.button == 0 && ev.mouse_button.pressed) {
                    Window *top = find_topmost_window_under_cursor(ev.mouse_button.x, ev.mouse_button.y);
                    if (top) {
                        focus_window(top);
                        dragging_window = top;
                        drag_offset_x = ev.mouse_button.x - top->x;
                        drag_offset_y = ev.mouse_button.y - top->y;
                    } else {
                        focused_window = NULL;
                        dragging_window = NULL;
                    }
                }
                if (ev.mouse_button.button == 0 && !ev.mouse_button.pressed) {
                    dragging_window = NULL;
                }
                break;
            }
        }
        if (!events_processed && damage_count == 0) {
            __asm__ volatile("hlt");
            continue;
        }
        uint64_t now = get_millis();
        if (now - last_frame_time < frame_delay_ms) {
            __asm__ volatile("hlt");
            continue;
        }
        last_frame_time = now;

        int dx, dy, dw, dh;
        bool damaged = get_merged_damage(&dx, &dy, &dw, &dh);
        if (!damaged) {
            continue;
        }

        wallpaper_draw_region(fb.backbuffer, fb.width, fb.height, dx, dy, dw, dh);

        // Draw windows (bottom to top)
        for (int wi = 0; wi < window_count; wi++) {
            Window *w = windows[wi];
            // Draw shadow if enabled
            if (w->has_shadow) {
                int sx = w->x + 5, sy = w->y + 5;
                int rx0 = (sx < dx) ? dx : sx;
                int ry0 = (sy < dy) ? dy : sy;
                int rx1 = (sx + w->width > dx+dw) ? dx+dw : sx + w->width;
                int ry1 = (sy + w->height > dy+dh) ? dy+dh : sy + w->height;
                if (rx1 > rx0 && ry1 > ry0) {
                    // Shadow blending uses the existing scalar blend_pixel function
                    for (int yyy = ry0; yyy < ry1; yyy++) {
                        for (int xxx = rx0; xxx < rx1; xxx++) {
                            uint32_t dst = fb.backbuffer[yyy * fb.width + xxx];
                            fb.backbuffer[yyy * fb.width + xxx] = blend_pixel(0x7F000000, dst);
                        }
                    }
                }
            }
            // Blur behind window if needed
            if (w->blur_background && g_blur_enabled) {
                int rx0 = (w->x < dx) ? dx : w->x;
                int ry0 = (w->y < dy) ? dy : w->y;
                int rx1 = (w->x + w->width > dx+dw) ? dx+dw : w->x + w->width;
                int ry1 = (w->y + w->height > dy+dh) ? dy+dh : w->y + w->height;
                if (rx1 > rx0 && ry1 > ry0) {
                    blur_region(rx0, ry0, rx1 - rx0, ry1 - ry0);
                }
            }
            // Compute intersection of window and damage region
            int rx0 = (w->x < dx) ? dx : w->x;
            int ry0 = (w->y < dy) ? dy : w->y;
            int rx1 = (w->x + w->width > dx+dw) ? dx+dw : w->x + w->width;
            int ry1 = (w->y + w->height > dy+dh) ? dy+dh : w->y + w->height;
            if (rx1 > rx0 && ry1 > ry0) {
                // Fast path: if window is fully opaque, do row-wise memcpy
                if (!w->transparent && w->opacity == 255) {
                    for (int row = ry0; row < ry1; row++) {
                        int win_src_x = rx0 - w->x;
                        int win_row = row - w->y;
                        memcpy(&fb.backbuffer[row * fb.width + rx0],
                               &w->buffer[win_row * w->width + win_src_x],
                               (rx1 - rx0) * sizeof(uint32_t));
                    }
                } else {
                    // Blended per-pixel drawing path (kept scalar to preserve blending quality)
                    for (int yyy = ry0; yyy < ry1; yyy++) {
                        for (int xxx = rx0; xxx < rx1; xxx++) {
                            int wx = xxx - w->x;
                            int wy = yyy - w->y;
                            uint32_t pix = w->buffer[wy * w->width + wx];
                            uint8_t pa = (pix >> 24) & 0xFF;
                            uint8_t final_a = (uint8_t)((pa * w->opacity) / 255);
                            pix = (pix & 0x00FFFFFF) | (final_a << 24);
                            if (final_a == 0) continue;
                            if (final_a == 255) {
                                fb.backbuffer[yyy * fb.width + xxx] = pix;
                            } else {
                                uint32_t dst = fb.backbuffer[yyy * fb.width + xxx];
                                fb.backbuffer[yyy * fb.width + xxx] = blend_pixel(pix, dst);
                            }
                        }
                    }
                }
            }
        }

        // Draw mouse cursor last
        {
            #define CURSOR_COLOR 0xFFFF0000
            int cx = mouse_x, cy = mouse_y;
            int c_left = cx - 3, c_right = cx + 3;
            int c_top  = cy - 3, c_bot = cy + 3;
            if (!(c_right < dx || c_bot < dy || c_left > dx+dw || c_top > dy+dh)) {
                for (int xx = -3; xx <= 3; xx++) {
                    int px = cx + xx;
                    int py = cy;
                    if (px >= dx && px < dx + dw && py >= dy && py < dy + dh) {
                        fb.backbuffer[py * fb.width + px] = CURSOR_COLOR;
                    }
                }
                for (int yy = -3; yy <= 3; yy++) {
                    int px = cx;
                    int py = cy + yy;
                    if (px >= dx && px < dx + dw && py >= dy && py < dy + dh) {
                        fb.backbuffer[py * fb.width + px] = CURSOR_COLOR;
                    }
                }
            }
        }

        // Copy damaged region to frontbuffer
        for (int yyy = dy; yyy < dy + dh; yyy++) {
            memcpy(fb.fb_addr + yyy * fb.width + dx,
                   fb.backbuffer + yyy * fb.width + dx,
                   dw * sizeof(uint32_t));
        }
        clear_damage();
    }
}
