#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char** argv)
{
    int unsafe = 1;
    if (argc > 1)
    {
        unsafe = (atoi(argv[1]) != 0);
    }
    printf("Running %s path\n", unsafe ? "UNSAFE (free(s+1))" : "SAFE (free(s))");
    size_t sz = 32;
    char* s = malloc(sz);
    if (!s)
    {
        perror("malloc");
        return 1;
    }
    memset(s, 0x41, sz);
    // fill with 'A'
    s[sz - 1] = '\0';
    printf("Allocated block at %p (size %zu)\n", (void*) s, sz);
    // show a few bytes around the base pointer
    printf("Bytes at base: %02x %02x %02x %02x\n", (unsigned char) s[0], (unsigned char) s[1], (unsigned char) s[2], (unsigned char) s[3]);
    if (unsafe)
    {
        printf("Calling free(s + 1) -> UB\n");
        free(s + 1);
    }
    else
    {
        printf("Calling free(s) -> defined\n");
        free(s);
    }
    printf("Attempting another allocation after free...\n");
    char* t = malloc(sz);
    if (!t)
    {
        perror("malloc after free");
        return 1;
    }
    memset(t, 0x42, sz);
    // fill with 'B'
    printf("New block at %p, first bytes: %02x %02x %02x %02x\n", (void*) t, (unsigned char) t[0], (unsigned char) t[1],
      (unsigned char) t[2], (unsigned char) t[3]);
    free(t);
    printf("Finished (no crash reported by program). Note: UB may have been detected by ASAN.\n");
    return 0;
}