#include <jee.h>
#include <jee/spi-sdcard.h>

using namespace jeeh;

void sdTest () {
    SpiSoft spi;

    // F7508-DK:
    //  PC8  D0  MISO
    //  PC9  D1
    //  PC10 D2
    //  PC11 D3  NSEL
    //  PC12 CLK SCLK
    //  PC13 Detect
    //  PD2  CMD MOSI
    spi.init("D2,C8,C12,C11");

    SdCard sd (spi);
    spi.rate = 100;
    sd.init();
    spi.rate = 1;

    uint8_t buf [512];
    for (auto i = 0; i < 500; ++i) {
        sd.readBlock(i, buf);
        if (buf[0] != 0) {
            printf("%d\n", i);
            dumpHex(buf, 32);
        }
    }
}

int main () {
    fastClock();
    printf("%s: sdcard @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    systick::init(1);

    sdTest();

    printf("done %d\n", systick::millis());
}
