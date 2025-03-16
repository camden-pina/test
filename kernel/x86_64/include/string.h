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

void *__memset_slow(void *dest, int val, size_t len);
void *__memset8(void *dest, uint8_t val, size_t len);
void *__memset32(void *dest, uint32_t val, size_t len);
void *__memset64(void *dest, uint64_t val, size_t len);

#endif