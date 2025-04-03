#ifndef BMP_DECODER_H
#define BMP_DECODER_H

#include <stdint.h>

/* BMPImage structure to hold decoded BMP pixel data */
typedef struct {
    uint32_t *data;  /* Pixel data in ARGB format */
    int width;
    int height;
} BMPImage;

/*
 * Decode a BMP file from the given file path.
 * Supports uncompressed 24-bit and 32-bit BMP images.
 * Returns a pointer to a BMPImage on success, or NULL on failure.
 * The caller is responsible for freeing the image using bmp_free().
 */
BMPImage* bmp_decode(const char *dfile_path);

/* Free the BMPImage and its associated pixel data */
void bmp_free(BMPImage *image);

#endif // BMP_DECODER_H
