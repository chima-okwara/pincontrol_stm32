// this is a send-only test of the dma-based uart driver for l432 and f413

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

void writer (Message&, void*) {
    os::delay(20);

  //auto s = " abcdefghijklmnopqrstuvwxyz + ABCDEFGHIJKLMNOPQRSTUVWXYZ /\n";
    auto s = "                                                         /\n";
    auto n = strlen(s);

    for (auto i = 1; i <= 500; ++i) {
        auto a = i % n;

        led = 1;

        Message msg {-2, 'W', (uint16_t) (n-a), (void*) (s+a)};
        os::call(msg);

        led = 0;
    }
}

int main () {
    //fastClock();
    printf("\n### %s: uart @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    uint32_t stack [250];
    Tasker tasker (stack);

    Uart uart;
    verify(~uart.devId == -2);
    uart.init(UART_CONSOLE);

    uint32_t writerStack [3][150];
    for (auto& e : writerStack)
        os::fork(e, writer);

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("Device: %d b, Uart: %d b, Config: %d b\n",
            sizeof (Device), sizeof (Uart), sizeof (Uart::Config));
    printf("done %d ms\n", systick::millis());
}
