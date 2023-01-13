#include <jee.h>

using namespace jeeh;

int main () {
    printf("%s: busyloop @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    cycles::init();
    systick::init();

    auto t = systick::micros();
    cycles::clear();
    msIdle(100);
    auto n = cycles::count();

    printf("100 ms = %d µs (systick), %5d cycles, %d%%\n",
            systick::micros() - t, n, n / (SystemCoreClock/1000));
}
