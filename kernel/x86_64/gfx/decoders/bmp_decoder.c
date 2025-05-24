#include "gfx/decoders/bmp_decoder.h"
#include <kernel.h>
#include <mm/pmm.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <panic.h>
#include <storage/partition.h>

/* BMP header structures (packed) */
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      /* 'BM' */
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPFileHeader;

typedef struct {
    uint32_t biSize;      
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

#include <storage/fs/vfs.h>

BMPImage* bmp_decode(const char *dfile_path) {

    char *file_path = "wp.bmp";
    // For this example, we'll assume device #0 is the raw disk.
    block_device_t *raw_disk = get_block_device(0);
    if (!raw_disk) {
        panic("Error: raw disk not found");
    }
    kprintf("Raw disk '%s' with %llu sectors.\n",
            raw_disk->name, (unsigned long long)raw_disk->sector_count);

    // Perform partition scanning on the raw disk.
    block_list();
    int pcount = partition_scan(raw_disk);
    if (pcount < 0) {
        panic("No valid partition table or parse error.");
    }
    if (pcount == 0) {
        kprintf("No partitions found on the raw disk.\n");
        return NULL;
    }
    kprintf("partition_scan found %d partition(s).\n", pcount);

    // After scanning, determine the new device count.
    int after_count = get_block_device_count();
    // Assume the last device registered is the one we want.
    int partition_index = after_count - 1;
    block_device_t *part_dev = get_block_device(partition_index);
    if (!part_dev) {
        panic("Couldn't get the newly registered partition device!");
    }
    kprintf("Attempting to mount FAT32 on device #%d ('%s').\n",
            partition_index, part_dev->name);

    // Mount the partition as the root filesystem under a mount point.
    if (vfs_mount("/drive0", part_dev, "fat32") != 0) {
        panic("Failed to mount the partition device as FAT32.");
    }
    
    // Now use VFS to open and read the BMP file.
    vfs_file_t *file = vfs_open("/drive0/wp.bmp", "r");
    if (!file) {
        kprintf("Failed to open BMP file '%s'\n", file_path);
        return NULL;
    }

        // Determine the file size. This assumes you have a method to get it.
    // (If not, you may need to read in a loop until EOF.)
    uint32_t file_size = vfs_file_size(file);
    if (file_size < sizeof(BMPFileHeader) + sizeof(BMPInfoHeader)) {
        kprintf("File too small to be a valid BMP: %s\n", file_path);
        vfs_close(file);
        return NULL;
    }
    kprintf("BMP file '%s' size: %u bytes.\n", file_path, file_size);

    // For example, read the file dynamically (assuming we have a way to get the file size).
    // Here we'll read it in chunks. Adjust as needed for your BMP parsing.
    uint8_t *buffer = (uint8_t*)kmalloc(file_size);
    if (!buffer) {
        kprintf("Memory allocation failed for BMP file reading.\n");
        vfs_close(file);
        return NULL;
    }

    uint32_t total_read = 0;
    int bytes_read = 0;
    int chunk_size = 4096;  // you can adjust the chunk size

    // Read until EOF or until we've read file_size bytes.
    kprintf("file_size: %llu\n", file_size);
    while (total_read < file_size) {
        int to_read = (file_size - total_read < chunk_size) ? file_size - total_read : chunk_size;
        bytes_read = vfs_read(file, buffer + total_read, to_read);
        if (bytes_read < 0) {
            kprintf("Error reading BMP file '%s'\n", file_path);
            kfree(buffer);
            vfs_close(file);
            return NULL;
        }
        // Break if EOF is reached
        if (bytes_read == 0) {
            break;
        }
        // kprintf("total_read: %llu\n", total_read);
        total_read += bytes_read;
    }

    if (total_read < file_size) {
        kprintf("Warning: Expected %u bytes, but only read %u bytes from '%s'\n",
            file_size, total_read, file_path);
    }

    // Do BMP decoding here using the read data...
    // For now, just print the first few bytes as a placeholder.
    kprintf("BMP file '%s' read %d bytes. First byte: 0x%02x\n", 
            file_path, bytes_read, buffer[0]); 

    // if (!filedata) {
      //   kprintf("[BMP Decoder] Failed to read file: %s\n", file_path);
        //return NULL;
   // }

    /* Check BMP signature */
    BMPFileHeader *bfh = (BMPFileHeader *)buffer;
    if (bfh->bfType != 0x4D42) {  /* 'BM' in little-endian */
        kprintf("[BMP Decoder] Invalid BMP signature in file: %s\n", file_path);
        kfree(buffer);
        return NULL;
    }
    BMPInfoHeader *bih = (BMPInfoHeader *)(buffer + sizeof(BMPFileHeader));

    /* Only support uncompressed BMP (BI_RGB) with 24 or 32 bits per pixel */
    if (bih->biCompression != 0 || (bih->biBitCount != 24 && bih->biBitCount != 32)) {
        kprintf("[BMP Decoder] Unsupported BMP format in file: %s\n", file_path);
        kfree(buffer);
        return NULL;
    }

    int width = bih->biWidth;
    int height = bih->biHeight;
    kprintf("bitmap width: %llu\n", width);
    kprintf("bitmap height: %llu\n", height);
    
    bool bottom_up = true;
    if (height < 0) {
        bottom_up = false;
        height = -height;
    }
    
    uint32_t *img_data = kmalloc(width * height * sizeof(uint32_t));
    if (!img_data) {
        kprintf("[BMP Decoder] Out of memory decoding BMP: %s\n", file_path);
        kfree(buffer);
        return NULL;
    }
    
    uint8_t *pixel_data = buffer + bfh->bfOffBits;
    int bytes_per_pixel = bih->biBitCount / 8;
    int row_size = (bih->biBitCount == 24)
        ? ((width * bytes_per_pixel + 3) / 4) * 4
        : width * 4;
    
    kprintf("bfType      = 0x%X\n", bfh->bfType);
    kprintf("bfSize      = %u bytes\n", bfh->bfSize);
    kprintf("bfOffBits   = %u\n", bfh->bfOffBits);
    kprintf("biWidth     = %d\n", bih->biWidth);
    kprintf("biHeight    = %d\n", bih->biHeight);
    kprintf("biBitCount  = %u\n", bih->biBitCount);
    kprintf("biCompression = %u\n", bih->biCompression);
    kprintf("Pixel data starts at offset %u\n", bfh->bfOffBits);
    kprintf("Row size: %d bytes\n", row_size);
    
    // Dump pixel data preview
    for (int i = 0; i < 32; i++) {
        kprintf("%02X ", pixel_data[i]);
    }
    kprintf("\n");
    
    for (int y = 0; y < height; y++) {
        int src_y = bottom_up ? (height - 1 - y) : y;
        uint8_t *row_ptr = pixel_data + src_y * row_size;
    
        for (int x = 0; x < width; x++) {
            int offset = x * bytes_per_pixel;
    
            uint8_t b = row_ptr[offset + 0];
            uint8_t g = row_ptr[offset + 1];
            uint8_t r = row_ptr[offset + 2];
            uint8_t a = (bytes_per_pixel == 4) ? row_ptr[offset + 3] : 0xFF;
    
            if (x < 3 && y < 3) {
                kprintf("Pixel(%d,%d): R:%02x G:%02x B:%02x A:%02x\n", x, y, r, g, b, a);
            }
    
            img_data[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    kfree(buffer);

    BMPImage *img = kmalloc(sizeof(BMPImage));
    if (!img) {
        kfree(img_data);
        return NULL;
    }
    img->data = img_data;
    img->width = width;
    img->height = height;

    vfs_close(file);

    return img;
}

void bmp_free(BMPImage *image) {
    if (image) {
        if (image->data)
            kfree(image->data);
        kfree(image);
    }
}
