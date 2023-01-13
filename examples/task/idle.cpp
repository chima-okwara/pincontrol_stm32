// a quick test of the new sleep approach w/o requiring an idle task

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

int main () {
#if STM32L0
    fastClock();
#endif
    printf("\n### %s: idle @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    uint32_t stack [250];
    Tasker tasker (stack);

    for (auto i = 0; i < 10; ++i) {
        os::delay(250); // this is new: the ability to suspend the main task
        led.toggle();
    }

//  while (Tasker::count > 0)
//      os::get();

    printf("done %d\n", systick::millis());
}
