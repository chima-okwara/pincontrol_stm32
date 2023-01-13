// minimal send + receive blocking, with brief delay to blink on-board led

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

void blip (Message&, void*) {
    printf("blip start\n");

    led = 1;
    os::delay(100);
    led = 0;

    printf("blip done\n");
}

int main () {
#if STM32L0
    fastClock();
#endif
    printf("\n### %s: blip @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    uint32_t stack [250];
    Tasker tasker (stack);

    uint32_t blipStack [150];
    os::fork(blipStack, blip);

    uint32_t *psp, *msp;
    asm volatile (
        " mrs %0,psp \n"
        " mrs %1,msp \n"
    : "=r" (psp), "=r" (msp));
    printf("PSP %10p MSP %10p\n", psp, msp);

    auto UID = (uint32_t const*) Signature::uid;
    printf("UID %08x %08x %08x\n", UID[0], UID[1], UID[2]);

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("done\n");
}
