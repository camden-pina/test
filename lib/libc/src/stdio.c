#include <stdio.h>
#include <unistd.h>
#include <string.h>
int puts(const char *s)
{
    // size_t len=strlen(s);
    // write(1,s,len);
    write(1,"ab",2);
    for(;;);
    return 2; // (int)5+1;
}

