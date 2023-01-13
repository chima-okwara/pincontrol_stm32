#include <jee.h>
#include <jee/qspi-f723.h>

using namespace jeeh;

int main () {
    fastClock();
    printf("%s: qspi @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    systick::init();
    qspi::init();

    const auto qmem = qspi::addr;

    for (int n = 0; n < 25; ++n) {
        printf("%2d: ", n);
        dumpHex(qmem + n*64);
    }
    
    qspi::deinit(); // TODO avoids lots of junk output on exit (why?)
}
