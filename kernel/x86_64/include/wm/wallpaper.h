#ifndef WALLPAPER_H
#define WALLPAPER_H

#include <stdint.h>
#include <stdbool.h>

/* Wallpaper display modes */
typedef enum {
    WALLPAPER_MODE_STRETCH,  // Scale image to fill the entire framebuffer
    WALLPAPER_MODE_CENTER,   // Center the image (with background fill)
    WALLPAPER_MODE_TILE      // Repeat the image across the framebuffer
} WallpaperMode;

/* Initialize the wallpaper subsystem.
 * Returns true on success.
 */
bool wallpaper_init(void);

/* Set a new wallpaper image from raw ARGB data.
 *
 * Parameters:
 *  - buffer: Pointer to a raw ARGB image.
 *  - width:  Width of the image.
 *  - height: Height of the image.
 *  - mode:   Display mode (stretch, center, tile).
 *
 * This function copies the image into its own internal buffer.
 * Returns true if the wallpaper was set successfully.
 */
bool wallpaper_set(uint32_t *buffer, int width, int height, WallpaperMode mode);

/* Set a new wallpaper from a file.
 *
 * The file can be in BMP, PNG, or JPEG format. BMP is fully supported,
 * while PNG and JPEG are stubbed for future implementation.
 *
 * Parameters:
 *  - file_path: Path to the image file.
 *  - mode:      Display mode (stretch, center, tile).
 *
 * Returns true if the wallpaper was set successfully.
 */
bool wallpaper_set_from_file(const char *file_path, WallpaperMode mode);

/* Draw the wallpaper onto the provided backbuffer.
 *
 * Parameters:
 *  - backbuffer: Pointer to the pixel buffer to draw to.
 *  - fb_width:   Framebuffer width.
 *  - fb_height:  Framebuffer height.
 *
 * Depending on the mode selected in wallpaper_set(), this function
 * will stretch, center, or tile the wallpaper image.
 */
void wallpaper_draw(uint32_t *backbuffer, int fb_width, int fb_height);

/*
 * Draws the wallpaper into only the specified rectangular region (rx,ry,rw,rh)
 * of the provided backbuffer. The fb_width and fb_height parameters are the full
 * dimensions of the backbuffer. This function supports STRETCH, CENTER, and TILE modes.
 */
void wallpaper_draw_region(uint32_t *backbuffer, int fb_width, int fb_height,
    int rx, int ry, int rw, int rh);

/* Free the wallpaper image resources, if any. */
void wallpaper_free(void);

#endif // WALLPAPER_H
