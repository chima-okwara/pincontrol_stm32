// example of a task being interrupted a few times while it is running

#include <jee.h>
using namespace jeeh;

void self (Message&, void*) {
    os::delay(10);

    printf("A %d\n", systick::millis());
    Message msg [] = { {-1, 0, 100}, {-1, 0, 200}, {-1, 0, 300} };
    os::put(msg[0]);
    os::put(msg[1]);
    os::put(msg[2]);

    printf("B %d\n", systick::millis());
    msBusy(250);

    printf("C %d\n", systick::millis());
    auto& r0 = os::get();
    verify(&r0 == &msg[0]);

    printf("D %d\n", systick::millis());
    auto& r1 = os::get();
    verify(&r1 == &msg[1]);

    printf("E %d\n", systick::millis());
    auto& r2 = os::get();
    verify(&r2 == &msg[2]);

    printf("F %d\n", systick::millis());
}

int main () {
    //fastClock();
    printf("\n### %s: self @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    uint32_t stack [250];
    Tasker tasker (stack);

    uint32_t selfStack [150];
    os::fork(selfStack, self);

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("done\n");
}
