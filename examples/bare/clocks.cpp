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
    printf("%s: clocks @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    msIdle(5); // drain uart

    cycles::init();

    led.mode("P"); // the following LED blinks will all be at the same rate

    auto hz1 = fastClock(true);
    auto cy1 = blinker();

    auto hz2 = fastClock(false);
    auto cy2 = blinker();

    auto hz3 = slowClock(true);
    auto cy3 = blinker();

    auto hz4 = slowClock(false);
    auto cy4 = blinker();

    // clock now back to power-up value, i.e. UART's baudrate is correct again

    printf("fast true:  %5u kHz, %8u cycles\n", hz1/1000, cy1);
    printf("fast false: %5u kHz, %8u cycles\n", hz2/1000, cy2);
    printf("slow true:  %5u kHz, %8u cycles\n", hz3/1000, cy3);
    printf("slow false: %5u kHz, %8u cycles\n", hz4/1000, cy4);

    led = 1;
    msIdle(100);

    powerDown(false); // stop mode 1, LED will stay on, uploads will work
    // true: standby, LED will turn off, uploads will fail when not in reset
}
