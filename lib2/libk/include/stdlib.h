#ifndef _KRNL_STDLIB_H
#define _KRNL_STDLIB_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

char* itoa(int num, char *buffer, int base);

#ifdef __cplusplus
}
#endif

#endif
