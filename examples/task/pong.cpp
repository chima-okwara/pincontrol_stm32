// measure the performance of a burst of send/receive round trips

#include <jee.h>
using namespace jeeh;

int count;
int8_t pongId, pingId;

void pong (Message&, void*) {
    printf("pong start\n");
    while (++count)
        os::put(os::get());
}

void ping (Message&, void*) {
    printf("ping loop\n");
    os::delay(10);

    auto t = cycles::count();
    for (auto i = 0; i < 1000; ++i) {
        Message msg {pongId};
        os::call(msg);
    }
    t = cycles::count() - t;

    auto mhz = SystemCoreClock/1'000'000;
    auto n = (int) ((SystemCoreClock * 1000ULL) / t);
    auto v = t / mhz;
    printf("%dx, %d cycles/call, %d calls/sec, %d.%03d µs/call @ %d MHz\n",
            count, t/1000, n, v/1000, v%1000, mhz);

    // make pong quit as well
    count = -1;
    Message msg2 {pongId};
    os::put(msg2);

    printf("ping done\n");
}

int main () {
#if STM32L0
    fastClock();
#endif
    printf("\n### %s: pong @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    cycles::init();

    uint32_t stack [250];
    Tasker tasker (stack);

    uint32_t pongStack [150];
    pongId = os::fork(pongStack, pong).mTag;

    uint32_t pingStack [150];
    pingId = os::fork(pingStack, ping).mTag;

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("done\n");
}
