// echo all incoming data back out, with multiple read buffers queued

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

void echo (Message&, void*) {
    Message m {-2, 'R'};
    os::put(m);

    while (true) {
        auto& msg = os::get();
        verify(msg.mTag == 'R' || msg.mTag == 'W');

        if (msg.mTag == 'R') {
            led = 1;
            msg.mTag = 'W';
            verify(msg.mLen > 0);
        } else {
            led = 0;
            msg.mTag = 'R';
        }

        verify(msg.mDst == -2);
        os::put(msg);
    }
}

int main () {
    //fastClock();
    printf("\n### %s: echo @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    uint32_t stack [250];
    Tasker tasker (stack);

    Uart uart;
    verify(~uart.devId == -2);
    uart.init(UART_CONSOLE);

    uint32_t echoStack [150];
    os::fork(echoStack, echo);

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("done %d ms\n", systick::millis());
}
