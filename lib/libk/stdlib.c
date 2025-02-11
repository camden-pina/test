#include <stdlib.h>
#include <string.h>

#define abs(N) ((N<0)?(-N):(N))

// inline function to swap two numbers
void swap(char* x, char* y) {
	char t = *x; *x = *y; *y = t;
}

// function to reverse buffer[i..j]
char* reverse(char* buffer, unsigned long long i, unsigned long long j)
{
	while (i < j)
		swap(&buffer[i++], &buffer[j--]);

	return buffer;
}

/* for ltoa() */
#define OCTAL       8
#define DECIMAL     10
#define HEX         16

char *ltoa(long long num, char *buffer, int radix)
{
    /* IBM says radix can only be the constants DECIMAL, OCTAL or HEX,
     * but nobody gives shit, we'll define those constants and still give
     * valid results if they are not used. */

    size_t i = 0;
    long divider = (long)radix;
    long tmp;

    int is_negative = 0;

    unsigned long long number;

    if(num < 0 && radix == DECIMAL)
    {
        is_negative = 1;
        i = 1;
        buffer[0] = '-';
        num *= -1;
        number = (unsigned long long)num;
    } else
    {
        number = (unsigned long long)num;
    }

    do
    {
        tmp = number % divider;
        if(tmp <= 9)
        {
            buffer[i] = tmp + '0';
        } else
        {
            buffer[i] = tmp - 10 + 'A';
        }

        number /= divider;
        i++;
    } while(number);

    buffer[i] = 0;

    /* reverse the string */
    i = strlen(buffer) - 1;
    size_t i2 = (size_t)is_negative;
    char tmpchar;

    while(i2 < i)
    {
        tmpchar = buffer[i2];
        buffer[i2] = buffer[i];
        buffer[i] = tmpchar;

        i2++;
        i--;
    }

    return buffer;
}

inline char* itoa(int num, char *buffer, int base)
{
    return ltoa((long long)num & 0xFFFFFFFF, buffer, base);
}

/*
// Iterative function to implement itoa() function in C
char* itoa(unsigned long long value, char* buffer, int base)
{
	// invalid input
	if (base < 2 || base > 32)
		return buffer;

	// consider absolute value of number
	unsigned long long n = abs(value);

	unsigned long long i = 0;
	while (n)
	{
		unsigned long long r = n % base;

		if (r > 9)
			buffer[i++] = 'a' + (r - 10);
		else
			buffer[i++] = '0' + r;
		
		n /= base;
	}

	// if number is 0
	if (i == 0)
		buffer[i++] = '0';

	// If base is 10 and value is negative, the resulting string 
	// is preceded with a minus sign (-)
	// With any other base, value is always considered unsigned
	if (value < 0 && base == 10)
		buffer[i++] = '-';

	buffer[i] = '\0'; // null terminate string

	// reverse the string and return it
	return reverse(buffer, 0, i - 1);
}
*/