#include <ctype.h>

/* 
 * Converts an uppercase ASCII letter to lowercase.
 * If the input is not an uppercase ASCII letter, it is returned unchanged.
 */
int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');  // Add 32
    }
    return c;
}
