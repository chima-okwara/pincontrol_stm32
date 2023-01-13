// extended interrupts, assumes the in & out gpio pins are connected together

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

#if STM32F103xB                 // bluepill
constexpr auto IN  = "A2";
constexpr auto OUT = "A3";
#elif STM32F103xE && PZ6806L    // PZ6806L
constexpr auto IN  = "B6";
constexpr auto OUT = "B7";
#elif STM32F302x8               // nucleo_f302r8
constexpr auto IN  = "C10";
constexpr auto OUT = "C11";
#elif STM32F303xE               // nucleo_f303ze
constexpr auto IN  = "C4";
constexpr auto OUT = "C5";
#elif STM32F407xx               // disco_f407vg
constexpr auto IN  = "B6";
constexpr auto OUT = "B7";
#elif STM32F723xx               // disco_f723ie
constexpr auto IN  = "A2";
constexpr auto OUT = "A3";
#elif STM32H745xx               // nucleo_h745zi_q
constexpr auto IN  = "B6";
constexpr auto OUT = "B7";
#elif STM32L011xx               // nucleo_l011k4
constexpr auto IN  = "A9";
constexpr auto OUT = "A10";
#elif STM32L031xx               // nucleo_l031k6
constexpr auto IN  = "A9";
constexpr auto OUT = "A10";
#elif STM32L053xx && JNZERO==4  // jeenode_zero v4
constexpr auto IN  = "A2";
constexpr auto OUT = "A3";
#elif STM32L432xx               // nucleo_l432kc
constexpr auto IN  = "A9";
constexpr auto OUT = "A10";
#elif STM32L496xx               // disco_l496ag
constexpr auto IN  = "H15"; // was G7, G-port not working?
constexpr auto OUT = "I11"; // was G8, G-port not working?
#endif

void exti (Message&, void*) {
    constexpr Pin in (IN);
    in.mode("F");

    Message msg {-2, IN[0], in.pin(), (void*) ExtIrq::BOTH};

    while (true) {
        os::call(msg); // configure and block until pin changes
        led.toggle();
    }
}

int main () {
#if STM32L0
    fastClock();
#endif
    printf("\n### %s: exti @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");
#if BLUEPILL | JNZERO | PZ6806L
    led.toggle();
#endif

    uint32_t stack [250];
    Tasker tasker (stack);

    ExtIrq extirq;
    verify(~extirq.devId == -2);

    constexpr Pin out (OUT);
    out.mode("P");

    uint32_t extiStack [150];
    os::fork(extiStack, exti);

    for (int i = 0; i < 5; ++i) {
        out = 1;
        os::delay(100);
        out = 0;
        os::delay(400);
    }

    printf("done %d ms\n", systick::millis());
}
