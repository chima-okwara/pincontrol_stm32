// multiple tasks, all delaying and switching around to idle in between

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

constexpr auto LOOPS = 10'000;

int counts;

void ticks (Message&, void*) {
    for (auto i = 0; i < LOOPS; ++i) {
        os::delay(1);
        ++counts;
    }
}

int main () {
#if STM32L0
    fastClock();
#endif
    printf("\n### %s: ticks @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    uint32_t stack [250];
    Tasker tasker (stack);

    // at 4 MHz, 8 tasks won't be able to complete in 10s, it takes some 20s
    // this can be seen by "idling" not appearing until the first tasks end
    Message msgs [] = { {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8} };

    constexpr int TASKS = sizeof msgs / sizeof *msgs;
    uint32_t tickStacks [TASKS][120];

    led = 1;
    for (auto i = 0; i < TASKS; ++i)
        os::fork(tickStacks[i], ticks);
    led = 0;

    printf("idling @ %d\n", systick::millis());
    while (tasker.count > 0)
        os::get();

    printf("done %d x %d = %d @ %d ms\n",
            TASKS, LOOPS, counts, systick::millis());
}
