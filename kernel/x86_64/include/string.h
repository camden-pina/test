#ifndef KERNEL_STRING_H
#define KERNEL_STRING_H
#include <kernel.h>

int memcmp(const void *str1, const void *str2, size_t count);
void *memcpy(void *dest, const void *src, size_t len);
void *memmove(void *dest, const void *src, size_t len);

void *memset(void *dest, int val, size_t len);

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int strlen(const char *s);
void *strcpy(char *dest, const char *src);
char *strdup(const char *s);
char *strcat(char *dest, const char *src);
/*
 * strncat:
 *   Appends at most n characters from the src string to the dest string.
 *   The destination string must be null-terminated and large enough to
 *   receive the additional characters, including the terminating null.
 *
 * Parameters:
 *   dest - Pointer to the destination string.
 *   src  - Pointer to the source string.
 *   n    - Maximum number of characters to append.
 *
 * Returns:
 *   A pointer to the destination string (dest).
 */
char *strncat(char *dest, const char *src, size_t n);

void *__memset_slow(void *dest, int val, size_t len);
void *__memset8(void *dest, uint8_t val, size_t len);
void *__memset32(void *dest, uint32_t val, size_t len);
void *__memset64(void *dest, uint64_t val, size_t len);

#endif