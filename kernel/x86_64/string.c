#include <string.h>
#include <panic.h>
#include <mm/pmm.h>
#include <ctype.h>
#include <immintrin.h>
#include <emmintrin.h>
#include <xmmintrin.h>

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
  
  void* memcpy(void* destination, const void* source, size_t num){
    //Q = 8 bytes at a time
    //D = 4 bytes at a time
    //W = 2 bytes at a time
    //B = 1 byte  at a time
    if(num % 8 == 0)
        __asm__ volatile("rep movsq" : : "D" (destination), "S" (source), "c" (num / 8));
    else if(num % 4 == 0)
        __asm__ volatile("rep movsd" : : "D" (destination), "S" (source), "c" (num / 4));
    else if(num % 2 == 0)
        __asm__ volatile("rep movsw" : : "D" (destination), "S" (source), "c" (num / 2));
    else
        __asm__ volatile("rep movsb" : : "D" (destination), "S" (source), "c" (num));
    return destination;
}
/*
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
    */


  void *memset(void *dest, int value, size_t len) {
    // Save the original pointer to return later.
    unsigned char *orig = (unsigned char *)dest;
    unsigned char *dst = orig;
    
    if (len == 0)
        return dest;
    
    // Handle the unaligned prefix: fill bytes until the pointer is 16-byte aligned.
    uintptr_t addr = (uintptr_t)dst;
    size_t misalignment = addr & 15;  // equivalent to addr % 16
    if (misalignment) {
        size_t prefix = 16 - misalignment;
        if (prefix > len)
            prefix = len;
        for (size_t i = 0; i < prefix; i++) {
            dst[i] = (unsigned char)value;
        }
        dst += prefix;
        len -= prefix;
    }
    
    // Now dst is 16-byte aligned.
    // Prepare a 128-bit SSE register with the byte value broadcasted across it.
    __m128i sse_val = _mm_set1_epi8((char)value);
    
    // Fill the main block using aligned SSE stores.
    size_t blocks = len / 16;
    for (size_t i = 0; i < blocks; i++) {
        _mm_store_si128((__m128i *)dst, sse_val);
        dst += 16;
    }
    
    // Fill any remaining bytes.
    size_t remainder = len % 16;
    for (size_t i = 0; i < remainder; i++) {
        dst[i] = (unsigned char)value;
    }
    
    // Return the original pointer as per the standard memset specification.
    return dest;
}

/*
  void* memcpy(void* destination, const void* source, size_t num){
    //Q = 8 bytes at a time
    //D = 4 bytes at a time
    //W = 2 bytes at a time
    //B = 1 byte  at a time
    if(num % 8 == 0)
        __asm__ volatile("rep movsq" : : "D" (destination), "S" (source), "c" (num / 8));
    else if(num % 4 == 0)
        __asm__ volatile("rep movsd" : : "D" (destination), "S" (source), "c" (num / 4));
    else if(num % 2 == 0)
        __asm__ volatile("rep movsw" : : "D" (destination), "S" (source), "c" (num / 2));
    else
        __asm__ volatile("rep movsb" : : "D" (destination), "S" (source), "c" (num));
    return destination;
}

*/
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
  
  /*
  void *memset(void *dest, int val, size_t len) {
    // return __memset8(dest, val, len);
    unsigned char *ptr = dest;
    while (len-- > 0) {
      *ptr++ = val;
    }
    return dest;
  }
  */

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

  char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;

    // Copy characters from src to dest until either n characters are copied
    // or the end of src is reached.
    for (; i < n && src[i] != '\0'; ++i) {
        dest[i] = src[i];
    }

    // If fewer than n characters were copied, pad the remainder of dest with '\0'
    for (; i < n; ++i) {
        dest[i] = '\0';
    }

    return dest;
}

int strcasecmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
      int c1 = tolower((unsigned char)*s1);
      int c2 = tolower((unsigned char)*s2);

      if (c1 != c2) {
          return c1 - c2;
      }

      s1++;
      s2++;
  }

  return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
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

  char *strtok_r(char *str, const char *delim, char **saveptr) {
    // If 'str' is NULL, we pick up where we left off from a previous call.
    if (!str) {
        str = *saveptr;
    }
    if (!str) {
        return NULL;
    }

    // Skip leading delimiters.
    str += strspn(str, delim);
    if (*str == '\0') {
        // No more tokens
        *saveptr = NULL;
        return NULL;
    }

    // The token begins here
    char *token_start = str;

    // Advance 'str' until we encounter a delimiter or the null terminator
    str += strcspn(str, delim);
    if (*str == '\0') {
        // Reached the end of this string
        *saveptr = NULL;
    } else {
        // Delimiter found -> terminate the token
        *str = '\0';
        *saveptr = str + 1;
    }

    return token_start;
}

size_t strspn(const char *s, const char *accept) {
  const char *p = s;
  while (*p) {
      const char *a = accept;
      bool match = false;
      while (*a) {
          if (*p == *a++) {
              match = true;
              break;
          }
      }
      if (!match) {
          // Found a character not in 'accept'
          return p - s;
      }
      p++;
  }
  return p - s;
}

size_t strcspn(const char *s, const char *reject) {
  const char *p = s;
  while (*p) {
      const char *r = reject;
      while (*r) {
          if (*p == *r++) {
              // Found a delimiter
              return p - s;
          }
      }
      p++;
  }
  return p - s;
}

char *strrchr(const char *s, int c) {
  const char *last = NULL;
  while (*s) {
      if (*s == (char)c) {
          last = s; 
      }
      s++;
  }
  // Check for '\0' if c == 0, so we can return pointer to the string's terminator
  if (c == '\0') {
      return (char*)s;
  }
  return (char*)last;
}

int toupper(int c) {
  if (c >= 'a' && c <= 'z') {
      return c - ('a' - 'A'); // or subtract 32
  }
  return c;
}
