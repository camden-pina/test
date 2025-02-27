/*
#ifndef _KRNL_STRING_H
#define _KRNL_STRING_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

void*  memcpy(void*, const void*, size_t);
void*  memset(void*, int, size_t);
int    memcmp(const void* aptr, const void* bptr, unsigned long long size);

char*  strcpy(char* dest, const char* src);
size_t strlen(const char* str);
char*  strcat(char* dest, const char* src);
char*  strchr(const char* str, int c);
int strcmp(const char *X, const char *Y);

int atoi(const char* str);

#ifdef __cplusplus
}
#endif

#endif
*/