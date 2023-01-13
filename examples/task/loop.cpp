// lots of tasks, looping, being busy, and asking for delays

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

// 0 = idle task, 1..N = looper, N+1 = reporter
#if STM32L0
constexpr auto N = 10;
#else
constexpr auto N = 30;
#endif

void looper (Message& m, void*) {
    auto id = m.mTag;
    os::delay(id+100);

    auto t = systick::millis();
    for (auto i = 0; i < 5; ++i) {
        for (auto j = 0; j < 100; ++j) {
            os::delay(1);

            Message msg {-1, 0, 1};
            os::put(msg);
            msBusy(2);
            auto& r = os::get();
            verify(&r == &msg);

            msBusy(1);
        }
        led.toggle();
    }
    printf("%d: %d ms\n", id, systick::millis() - t);
}

void reporter (Message&, void*) {
    for (auto i = 0; i < 5; ++i) {
        os::delay(10'000);
        os::showTasks();
    }
}

int main () {
#if STM32L0
    fastClock();
#endif
    printf("\n### %s: loop @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    uint32_t stacks [N+2][120];
    Tasker tasker (stacks[0]);

    for (auto i = 1; i <= N; ++i)
        os::fork(stacks[i], looper);
    os::fork(stacks[N+1], reporter);

    os::showTasks();

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("done %d ms\n", systick::millis());
}
