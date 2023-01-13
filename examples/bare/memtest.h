// Adapted from https://github.com/PaulStoffregen/teensy41_psram_memtest/

uint32_t* extRamStart;
uint32_t* extRamEnd;
int errors;

uint32_t const lfsrPatterns [] {
    2976674124, 1438200953, 3413783263, 1900517911, 1227909400,  276562754,
     146878114,  615545407,  110497896,   74539250, 4197336575, 2280382233,
     542894183, 3978544245, 2315909796, 3736286001, 2876690683,  215559886,
     539179291,  537678650, 4001405270, 2169216599, 4036891097, 1535452389,
    2959727213, 4219363395, 1036929753, 2125248865, 3177905864, 2399307098,
    3847634607,   27467969,  520563506,  381313790, 4174769276, 3932189449,
    4079717394,  868357076, 2474062993, 1502682190, 2471230478,   85016565,
    1427530695, 1100533073,
};

uint32_t const fixedPatterns [] {
    0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF, 0xAAAAAAAA,
    0xCCCCCCCC, 0xF0F0F0F0, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF, 0x00000000,
};

void fail (uint32_t* ptr, uint32_t got, uint32_t want) {
    printf("\nError at %p: read %08x != expected %08x\n", ptr, got, want);
    ++errors;
}

// store and check the same value in each memory location
void checkFixed (uint32_t pattern) {
    for (volatile auto p = extRamStart; p < extRamEnd; ++p)
        *p = pattern;

    for (volatile auto p = extRamStart; p < extRamEnd; ++p)
        if (auto actual = *p; actual != pattern)
            return fail(p, actual, pattern);

    printf("+");
}

// use a Linear-feedback shift register to generate a pseudo-random pattern
// see https://en.wikipedia.org/wiki/Linear-feedback_shift_register
void checkLfsr (uint32_t seed) {
    for (int n = 0; n < 2; ++n) {
        auto pattern = seed;
        for (volatile auto p = extRamStart; p < extRamEnd; p++) {
            if (n == 0)
                *p = pattern;
            else if (auto actual = *p; actual != pattern)
                return fail(p, actual, pattern);
            // note: this never sets pattern's bit 31 !
            for (int i = 0; i < 3; i++) {
                auto b = pattern & 1;
                pattern >>= 1;
                if (b)
                    pattern ^= 0x7A5BC2E3;
            }
        }
    }
    printf("*");
}

int memTests (uint32_t start, uint32_t bytes) {
    extRamStart = (uint32_t*) start;
    extRamEnd = (uint32_t*) (start + bytes);
    errors = 0;

    checkFixed(0x5A698421);
    for (auto e : lfsrPatterns)
        checkLfsr(e);
    for (auto e : fixedPatterns)
        checkFixed(e);

    return errors;
}
