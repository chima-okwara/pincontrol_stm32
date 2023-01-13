#include <jee.h>

using namespace jeeh;

Pin led ("B3");

uint32_t blinker () {
    cycles::clear();
    for (int i = 0; i < 10; ++i) {
        msIdle(100);
        led.toggle();
    }
    return cycles::count();
}

int main () {
    fastClock();

    printf("%s: clocks @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    msIdle(5); // drain uart

    led.mode("P"); // the following LED blinks will all be at the same rate
    systick::init(1, [](uint32_t) {
        led.toggle();
    });

    systick::msBusy(10000);
}
