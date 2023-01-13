// count using messages to send a "carry" to the next task, show value on leds
// sample output:
//
//  ### STM32L4x2: count @ 4 MHz
//  done 508 pulses, 2590 ms
//
// this corresponds to 7 blinking LEDs: 256+128+64+32+16+8+4 = 508

#include <jee.h>
using namespace jeeh;

int pulses;

void counter (Message& info, void* arg) {
    auto me = info.mTag;

    Pin led ((char const*) arg);
    led.mode("P");

    while (true) {
        auto& carry = os::get();
        ++pulses;

        led.toggle();

        if (led == 0 && me < 7) {
            Message msg {(int8_t) (me+1)};
            os::call(msg);
        }

        os::put(carry);
    }
}

int main () {
    //fastClock();
    printf("\n### %s: count @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    uint32_t stacks [8][150];
    Tasker tasker (stacks[0]);

    os::fork(stacks[1], counter, (void*) "A0");
    os::fork(stacks[2], counter, (void*) "A1");
    os::fork(stacks[3], counter, (void*) "A3");
    os::fork(stacks[4], counter, (void*) "A4");
    os::fork(stacks[5], counter, (void*) "A5");
    os::fork(stacks[6], counter, (void*) "A6");
    os::fork(stacks[7], counter, (void*) "B3");

    for (auto i = 0; i < 256; ++i) {
        Message msg {1};
        os::call(msg);
        os::delay(10);
    }

    printf("done %d pulses, %d ms\n", pulses, systick::millis());
}
