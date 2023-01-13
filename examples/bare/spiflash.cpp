#include <jee.h>
#include <jee/spi-flash.h>

using namespace jeeh;

int main () {
    fastClock();
    printf("%s: spiflash @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    Pin::config("E2:U,D13:U"); // pull qspi d2 & d3 high

    SpiSoft spi;
    SpiFlash spif (spi);
    spi.init("C9,C10,B2,B6");

    printf("id %06x, %dK\n", spif.devId(), spif.size());

    spif.erase(0);
    printf("sector 0 erased\n");

    uint8_t buf [256];
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 256; ++j)
            buf[j] = i + j;
        spif.write256(i, buf);
    }
    printf("10 pages written\n");

    for (int i = 0; i < 10; ++i) {
        printf("page %d:", i);
        spif.read256(i, buf);
        for (int j = 0; j < 11; ++j)
            printf(" %d", buf[j]);
        printf("\n");
    }

    //spif.wipe();
    //printf("wiped\n");

    spif.read256(0, buf);
    printf("buf[100] = %d\n", buf[100]);

    printf("done\n");
}
