#include "wm/wallpaper.h"
#include "gfx/decoders/bmp_decoder.h"  // Include the BMP decoder interface
#include <kernel.h>
#include <mm/pmm.h>
#include <string.h>
#include <stdint.h>

/* --- Internal Helpers --- */

/* Helper: Check if a string ends with a given suffix (case-insensitive) */
static bool str_endswith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return false;
    /* For simplicity, use strcasecmp; assume it’s available in your kernel */
    return (strcasecmp(str + str_len - suffix_len, suffix) == 0);
}

/* --- Wallpaper Data and Functions --- */

static struct {
    uint32_t *buffer;
    int width;
    int height;
    WallpaperMode mode;
} wallpaper = {0};

bool wallpaper_init(void) {
    wallpaper.buffer = NULL;
    wallpaper.width = 0;
    wallpaper.height = 0;
    wallpaper.mode = WALLPAPER_MODE_STRETCH;
    return true;
}

bool wallpaper_set(uint32_t *buffer, int width, int height, WallpaperMode mode) {
    /* Free previous wallpaper if any */
    if (wallpaper.buffer) {
        kfree(wallpaper.buffer);
        wallpaper.buffer = NULL;
    }
    size_t size = width * height * sizeof(uint32_t);
    wallpaper.buffer = kmalloc(size);
    if (!wallpaper.buffer) {
        kprintf("[Wallpaper] Failed to allocate memory for wallpaper\n");
        return false;
    }
    memcpy(wallpaper.buffer, buffer, size);
    wallpaper.width = width;
    wallpaper.height = height;
    wallpaper.mode = mode;
    return true;
}

bool wallpaper_set_from_file(const char *file_path, WallpaperMode mode) {
    /* Only BMP is fully implemented at this time */
    BMPImage *img = NULL;
    if (str_endswith(file_path, ".bmp") || str_endswith(file_path, ".BMP")) {
         img = bmp_decode(file_path);
    } else if (str_endswith(file_path, ".png") || str_endswith(file_path, ".PNG")) {
         kprintf("[Wallpaper] PNG decoding not yet implemented: %s\n", file_path);
         return false;
    } else if (str_endswith(file_path, ".jpg") || str_endswith(file_path, ".jpeg") ||
               str_endswith(file_path, ".JPG") || str_endswith(file_path, ".JPEG")) {
         kprintf("[Wallpaper] JPEG decoding not yet implemented: %s\n", file_path);
         return false;
    } else {
         kprintf("[Wallpaper] Unsupported file format: %s\n", file_path);
         return false;
    }
    if (!img) {
         kprintf("[Wallpaper] Failed to decode image: %s\n", file_path);
         return false;
    }
    bool result = wallpaper_set(img->data, img->width, img->height, mode);
    bmp_free(img);
    return result;
}

/* Helper to fill a rectangle in the backbuffer with a solid color */
static void fill_rect(uint32_t *buffer, int fb_width, int fb_height,
                      int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb_width)
        w = fb_width - x;
    if (y + h > fb_height)
        h = fb_height - y;
    if (w <= 0 || h <= 0)
        return;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            buffer[yy * fb_width + xx] = color;
        }
    }
}

/* Draw the wallpaper onto the given backbuffer.
 * For WALLPAPER_MODE_STRETCH, the image is scaled (nearest-neighbor).
 * For WALLPAPER_MODE_CENTER, the background is filled with a default dark gray,
 *   and then the image is drawn centered.
 * For WALLPAPER_MODE_TILE, the image is repeated to cover the screen.
 */
void wallpaper_draw(uint32_t *backbuffer, int fb_width, int fb_height) {
    if (!wallpaper.buffer) {
        fill_rect(backbuffer, fb_width, fb_height, 0, 0, fb_width, fb_height, 0xFF202020);
        return;
    }

    switch (wallpaper.mode) {
        case WALLPAPER_MODE_STRETCH:
            for (int y = 0; y < fb_height; y++) {
                int src_y = y * wallpaper.height / fb_height;
                for (int x = 0; x < fb_width; x++) {
                    int src_x = x * wallpaper.width / fb_width;
                    backbuffer[y * fb_width + x] = wallpaper.buffer[src_y * wallpaper.width + src_x];
                }
            }
            break;

        case WALLPAPER_MODE_CENTER: {
            fill_rect(backbuffer, fb_width, fb_height, 0, 0, fb_width, fb_height, 0xFF202020);
            int start_x = (fb_width - wallpaper.width) / 2;
            int start_y = (fb_height - wallpaper.height) / 2;
            for (int y = 0; y < wallpaper.height; y++) {
                int dest_y = start_y + y;
                if (dest_y < 0 || dest_y >= fb_height)
                    continue;
                uint32_t *dest = &backbuffer[dest_y * fb_width + ((start_x < 0) ? 0 : start_x)];
                uint32_t *src = &wallpaper.buffer[y * wallpaper.width];
                int copy_width = wallpaper.width;
                if (start_x < 0) {
                    src -= start_x;
                    copy_width += start_x;
                }
                if (start_x + copy_width > fb_width) {
                    copy_width = fb_width - start_x;
                }
                memcpy(dest, src, copy_width * sizeof(uint32_t));
            }
            break;
        }

        case WALLPAPER_MODE_TILE:
            for (int y = 0; y < fb_height; y++) {
                for (int x = 0; x < fb_width; x++) {
                    int src_x = x % wallpaper.width;
                    int src_y = y % wallpaper.height;
                    backbuffer[y * fb_width + x] = wallpaper.buffer[src_y * wallpaper.width + src_x];
                }
            }
            break;

        default:
            break;
    }
}

/*
 * Draws the wallpaper into only the specified rectangular region (rx,ry,rw,rh)
 * of the provided backbuffer. The fb_width and fb_height parameters are the full
 * dimensions of the backbuffer. This function supports STRETCH, CENTER, and TILE modes.
 */
void wallpaper_draw_region(uint32_t *backbuffer, int fb_width, int fb_height,
    int rx, int ry, int rw, int rh) {
if (!wallpaper.buffer) {
fill_rect(backbuffer, fb_width, fb_height, rx, ry, rw, rh, 0xFF202020);
return;
}

switch (wallpaper.mode) {
case WALLPAPER_MODE_STRETCH: {
// For each pixel in the damaged region, compute the corresponding source pixel.
for (int y = 0; y < rh; y++) {
int dest_y = ry + y;
if (dest_y < 0 || dest_y >= fb_height)
continue;
int src_y = dest_y * wallpaper.height / fb_height;
for (int x = 0; x < rw; x++) {
int dest_x = rx + x;
if (dest_x < 0 || dest_x >= fb_width)
 continue;
int src_x = dest_x * wallpaper.width / fb_width;
backbuffer[dest_y * fb_width + dest_x] = wallpaper.buffer[src_y * wallpaper.width + src_x];
}
}
break;
}
case WALLPAPER_MODE_CENTER: {
// Fill the region with a default dark gray first.
fill_rect(backbuffer, fb_width, fb_height, rx, ry, rw, rh, 0xFF202020);
int start_x = (fb_width - wallpaper.width) / 2;
int start_y = (fb_height - wallpaper.height) / 2;
// Compute the intersection of the damaged region and the centered wallpaper.
int inter_left   = (rx > start_x) ? rx : start_x;
int inter_top    = (ry > start_y) ? ry : start_y;
int inter_right  = ((rx + rw) < (start_x + wallpaper.width)) ? (rx + rw) : (start_x + wallpaper.width);
int inter_bottom = ((ry + rh) < (start_y + wallpaper.height)) ? (ry + rh) : (start_y + wallpaper.height);
if (inter_right > inter_left && inter_bottom > inter_top) {
for (int y = inter_top; y < inter_bottom; y++) {
int src_y = y - start_y;
for (int x = inter_left; x < inter_right; x++) {
 int src_x = x - start_x;
 backbuffer[y * fb_width + x] = wallpaper.buffer[src_y * wallpaper.width + src_x];
}
}
}
break;
}
case WALLPAPER_MODE_TILE: {
// For each pixel in the region, map it to the wallpaper image using modulo arithmetic.
for (int y = 0; y < rh; y++) {
int dest_y = ry + y;
if (dest_y < 0 || dest_y >= fb_height)
continue;
int src_y = dest_y % wallpaper.height;
for (int x = 0; x < rw; x++) {
int dest_x = rx + x;
if (dest_x < 0 || dest_x >= fb_width)
 continue;
int src_x = dest_x % wallpaper.width;
backbuffer[dest_y * fb_width + dest_x] = wallpaper.buffer[src_y * wallpaper.width + src_x];
}
}
break;
}
default:
fill_rect(backbuffer, fb_width, fb_height, rx, ry, rw, rh, 0xFF202020);
break;
}
}

void wallpaper_free(void) {
    if (wallpaper.buffer) {
        kfree(wallpaper.buffer);
        wallpaper.buffer = NULL;
        wallpaper.width = 0;
        wallpaper.height = 0;
    }
}
