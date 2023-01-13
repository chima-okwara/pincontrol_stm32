// ram test for PSRAM on the F723 Discovery board

#include <jee.h>
using namespace jeeh;
#include "memtest.h"
#include <cstdio> // for setbuf

void initFsmcPins () {
    RCC(EN_FMC, 1) = 1;

    Pin::config("D0:V12,D1,D4,D5,D7,D8,D9,D10,D11,D12,D14,D15,"
                "E0,E1,E7,E8,E9,E10,E11,E12,E13,E14,E15,"
                "F0,F1,F2,F3,F4,F5,F12,F13,F14,F15,"
                "G0,G1,G2,G3,G4,G5,G9");
}

void initPsram () {
    enum { BCR1=0x00, BTR1=0x04 };

    FMC[BCR1] = (1<<20) | (1<<19) | (1<<12) | (1<<8) | (1<<7) | (1<<4);
    FMC[BTR1] = (1<<20) | (6<<8) | (2<<4) | (9<<0);
    FMC[BCR1](0) = 1; // MBKEN
}

int main () {
    fastClock();
    printf("%s: psram @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    constexpr uint32_t addr = 0x6000'0000, size = 512; // kB
    printf("memory test: %d kB @ 0x%08x\n", size, addr);

    initFsmcPins();
    initPsram();
    setbuf(stdout, nullptr); // disable line buffering

    while (true) {
        auto e = memTests(addr, size * 1024);
        printf(" %d errors\n", e);
    }
}
