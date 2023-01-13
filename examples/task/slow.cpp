// implement a slow 4 Hz timer chain using a separate task

#include <jee.h>
using namespace jeeh;
#include "board.h"

void slow (Message&, void*) {
    constexpr Pin led (LED_PIN);
    led.mode("P");

    static uint32_t prev;

    while (true) {
        led.toggle();
        auto t = systick::millis();
        printf("%4d @ %d ms\n", t - prev, t);
        os::delay(250 - t % 250);
        prev = t;
    }
}

void busy (Message&, void*) {
    while (true) {
        msBusy(90);
        os::delay(600);
    }
}

int main () {
#if STM32L0
    fastClock();
#endif
    printf("\n### %s: slow @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    uint32_t stack [250];
    Tasker tasker (stack);

    uint32_t slowStack [150];
    os::fork(slowStack, slow);

    uint32_t busyStack [150];
    os::fork(busyStack, busy);

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("done\n");
}
