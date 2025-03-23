#include <string.h>
#include <panic.h>
#include <mm/pmm.h>

int memcmp(const void *str1, const void *str2, size_t count) {
    const unsigned char *s1 = str1;
    const unsigned char *s2 = str2;
  
    while (count-- > 0) {
      if (*s1++ != *s2++) {
        return s1[-1] < s2[-1] ? -1 : 1;
      }
    }
    return 0;
  }
  
  void *memcpy(void *dest, const void *src, size_t len) {
    if (len == 0) {
      return NULL;
    }
  
    char *d = dest;
    const char *s = src;
    while (len--) {
      *d++ = *s++;
    }
    return dest;
  }
  
  void *memmove(void *dest, const void *src, size_t len) {
    if (len == 0) {
      return dest;
    }
  
    char *d = dest;
    const char *s = src;
    if (d < s) {
      while (len--) {
        *d++ = *s++;
      }
    } else {
      char *lasts = s + (len - 1);
      char *lastd = d + (len - 1);
      while (len--) {
        *lastd-- = *lasts--;
      }
    }
    return dest;
  }
  
  void *memset(void *dest, int val, size_t len) {
    // return __memset8(dest, val, len);
    unsigned char *ptr = dest;
    while (len-- > 0) {
      *ptr++ = val;
    }
    return dest;
  }

  int strcmp(const char *s1, const char *s2) {
    while (*s1) {
      if (*s1 != *s2) break;
      s1++;
      s2++;
    }
  
    return *s1 - *s2;
  }
  
  // Adapted from: https://github.com/gcc-mirror/gcc/blob/master/libiberty/strncmp.c
  int strncmp(const char *s1, const char *s2, size_t n) {
    unsigned char u1, u2;
  
    while (n-- > 0) {
      u1 = (unsigned char) *s1++;
      u2 = (unsigned char) *s2++;
      if (u1 != u2) {
        return u1 - u2;
      }
  
      if (u1 == '\0') {
        return 0;
      }
    }
    return 0;
  }
  
  int strlen(const char *s) {
    int len = 0;
    while (*s != 0) {
      len++;
      s++;
    }
    return len;
  }
  
  void *strcpy(char *dest, const char *src) {
    size_t len = strlen(src);
    char *d = dest;
    const char *s = src;
    while (len--) {
      *d++ = *s++;
    }
    *d = '\0';
    return dest;
  }

  char *strdup(const char *s) {
    if (!s)
        return NULL;

    size_t len = strlen(s);
    char *dup = kmalloc(len + 1);
    if (!dup)
        return NULL;

    memcpy(dup, s, len + 1);
    return dup;
}

/*
 * strcat - concatenate two strings
 *
 * Appends the src string to the dest string, overwriting the terminating null
 * byte ('\0') at the end of dest, and then adds a terminating null byte.
 *
 * Parameters:
 *   dest - a pointer to the destination string buffer (must be large enough)
 *   src  - a pointer to the source string to append
 *
 * Returns:
 *   The pointer to the destination string dest.
 */
char *strcat(char *dest, const char *src) {
  char *d = dest;
  // Move d to the end of the destination string.
  while (*d) {
      d++;
  }
  // Copy src to the end of dest.
  while (*src) {
      *d++ = *src++;
  }
  // Append null terminator.
  *d = '\0';
  return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
  char *d = dest;
  
  // Move d to the end of the destination string.
  while (*d != '\0') {
      d++;
  }
  
  // Append up to n characters from src.
  size_t i = 0;
  while (i < n && src[i] != '\0') {
      d[i] = src[i];
      i++;
  }
  
  // Null terminate the destination string.
  d[i] = '\0';
  
  return dest;
}
