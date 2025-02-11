#include <string.h>

void* memcpy(void* destination, const void* source, size_t length)
{
	for (unsigned long long i = 0; i < length; ++i) {
		((unsigned char*)destination)[i] = ((unsigned char*)source)[i];
	}
	return destination;
}

void* memset(void* buffer, int value, size_t length)
{
	for (unsigned long long i = 0; i < length; ++i) {
		((unsigned char*)buffer)[i] = (unsigned char)value;
	}
	return buffer;
}

int memcmp(const void* aptr, const void* bptr, unsigned long long size)
{
	const unsigned char* a = (const unsigned char*)aptr;
	const unsigned char* b = (const unsigned char*)bptr;
	for (unsigned long long i = 0; i < size; i++) {
		if (a[i] < b[i])
			return -1;
		else if (b[i] < a[i])
			return 1;
	}
	return 0;
}

char* strcpy(char* dest, const char* src)
{
	while (*src !='\0')
	{
		*(dest) = *(src);
		src++;
		dest++;
	}
	*dest = '\0';
	return dest;
}

int strcmp(const char *X, const char *Y)
{
    while(*X)
    {
        // if characters differ or end of second string is reached
        if (*X != *Y)
            break;
 
        // move to next pair of characters
        X++;
        Y++;
    }
 
    // return the ASCII difference after converting char* to unsigned char*
    return *(const unsigned char*)X - *(const unsigned char*)Y;
}

size_t strlen(const char* str)
{
	size_t i  = 0;
	while (str[i])
		i++;
	return i;
}

char* strcat(char* dest, const char* src)
{
    char* ptr = dest + strlen(dest);
 
    while (*src != '\0')
        *ptr++ = *src++;
 
    *ptr = '\0';
 
    return dest;
}

char* strchr(const char* str, int c)
{
	return (void*)0;
}

// A simple atoi() function
int atoi(const char* str)
{
    // Initialize result
    int res = 0;
 
    // Iterate through all characters
    // of input string and update result
    // take ASCII character of corosponding digit and
    // subtract the code from '0' to get numerical
    // value and multiply res by 10 to shuffle
    // digits left to update running total
    for (int i = 0; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';
 
    // return result.
    return res;
}