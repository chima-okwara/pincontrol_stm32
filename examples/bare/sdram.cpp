// ram test for SDRAM on the F750 Discovery board

#include <jee.h>
using namespace jeeh;
#include "memtest.h"
#include <cstdio> // for setbuf

void initFsmcPins () {
    RCC(EN_FMC, 1) = 1;

    Pin::config("C3:PUV12,"
                "D0,D1,D8,D9,D10,D14,D15,"
                "E0,E1,E7,E8,E9,E10,E11,E12,E13,E14,E15,"
                "F0,F1,F2,F3,F4,F5,F11,F12,F13,F14,F15,"
                "G0,G1,G4,G5,G8,G15,"
                "H3,H5");
}

void initSdram () {
    enum { BCR1=0x00, BTR1=0x04 };

    FMC[BCR1] = (1<<20) | (1<<19) | (1<<12) | (1<<8) | (1<<7) | (1<<4);
    FMC[BTR1] = (1<<20) | (6<<8) | (2<<4) | (9<<0);
    FMC[BCR1](0) = 1; // MBKEN

    // SDRAM timing
    enum { CR1=0x140, TR1=0x148, CMR=0x150, RTR=0x154, SR=0x158 };
    FMC[CR1] = (0<<13)|(1<<12)|(2<<10)|(0<<9)|(2<<7)|(1<<6)|(1<<4)|(1<<2)|(0<<0);
    FMC[TR1] = (1<<24)|(1<<20)|(1<<16)|(6<<12)|(3<<8)|(6<<4)|(1<<0);

    // SDRAM commands
    auto fmcWait = []() { while (FMC[SR](5)) {} };
    fmcWait(); FMC[CMR] = (0<<9)|(0<<5)|(1<<4)|(1<<0);      // clock enable
    msIdle(2);
    fmcWait(); FMC[CMR] = (0<<9)|(0<<5)|(1<<4)|(2<<0);      // precharge
    fmcWait(); FMC[CMR] = (0<<9)|(7<<5)|(1<<4)|(3<<0);      // auto-refresh
    fmcWait(); FMC[CMR] = (0x220<<9)|(0<<5)|(1<<4)|(4<<0);  // load mode
    fmcWait(); FMC[RTR] = (0x0603<<1);
}

int main () {
    fastClock();
    printf("%s: sdram @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    constexpr uint32_t addr = 0xC000'0000, size = 8192; // kB
    printf("memory test: %d kB @ 0x%08x\n", size, addr);

    initFsmcPins();
    initSdram();
    setbuf(stdout, nullptr); // disable line buffering

    while (true) {
        auto e = memTests(addr, size * 1024);
        printf(" %d errors\n", e);
    }
}
