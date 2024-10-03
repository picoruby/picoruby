#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t c_rng_random_byte_impl(void)
{
    FILE *f;
    uint8_t byte;

    f = fopen("/dev/urandom", "rb");
    if (f == NULL) {
        fprintf(stderr, "Error: Unable to open /dev/urandom\n");
        exit(1);
    }

    if (fread(&byte, 1, 1, f) != 1) {
        fprintf(stderr, "Error: Unable to read from /dev/urandom\n");
        fclose(f);
        exit(1);
    }

    fclose(f);
    return byte;
}
