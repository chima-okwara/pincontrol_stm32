// the nucleo-f413zh has many uarts and timers to perform irq torture tests

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

void writer (Message&, void*) {
    auto& m = os::get();
    auto dev = m.mTag;

  //auto s = " abcdefghijklmnopqrstuvwxyz + ABCDEFGHIJKLMNOPQRSTUVWXYZ /\n";
    auto s = "                                                         /\n";
    auto n = strlen(s);

    auto t = systick::millis();
    for (auto i = 0; i < 500; ++i) {
        auto a = i % n;

        led = 1;

        Message msg {dev, 'W', (uint16_t) (n-a), (void*) (s+a)};
        os::call(msg);

        led = 0;
    }
    printf("%d: %d ms\n", ~dev, systick::millis() - t);
}

void reporter (Message&, void*) {
    for (auto i = 0; i < 3; ++i) {
        os::delay(200);
        os::showTasks();
    }
}

const auto EN_UART10 = 7 + 8 * APB2ENR; // missing from SVD definitions

// TODO APB1 bus has to use 25 MHz iso 50 MHz to get the proper baudrate
// FIXME likewise, the APB2 bus is 50 MHz iso 100 MHz ... no idea why!
// (the dividers have to be right, since UART3 is the 115200 baud console)

static Uart::Config const uartConfigs [] = {
    UART3_CONFIG,
    UART1_CONFIG,
    UART2_CONFIG,
    UART4_CONFIG,
    UART5_CONFIG,
    UART6_CONFIG,
    UART10_CONFIG,
};

int main () {
    //fastClock();
    printf("\n### %s: f413u @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    uint32_t stack [500];
    Tasker tasker (stack);

    constexpr auto N = sizeof uartConfigs / sizeof *uartConfigs;
    Uart uarts [N];

    uarts[0].init(uartConfigs[0], "D8:7,D9",  115200);  // dev -2 = uart 3
    uarts[1].init(uartConfigs[1], "A9:7,A10", 460800);  // dev -3 = uart 1
    uarts[2].init(uartConfigs[2], "A2:7,A3",  460800);  // dev -4 = uart 2
    uarts[3].init(uartConfigs[3], "A0:8,A1",  460800);  // dev -5 = uart 4
    uarts[4].init(uartConfigs[4], "C12:8,D2", 460800);  // dev -6 = uart 5
    uarts[5].init(uartConfigs[5], "C6:8,C7",  460800);  // dev -7 = uart 6
    uarts[6].init(uartConfigs[6], "E3:11,E2", 460800);  // dev -8 = uart 10

    uint32_t taskStacks [N][150];
    for (auto i = 0U; i < N-1; ++i) // task  (1..N) writes to device -i-1
        os::fork(taskStacks[i], writer);
    os::fork(taskStacks[N-1], reporter);

    Message msgs [N] = {
        {1, -3}, {2, -4}, {3, -5}, {4, -6}, {5, -7}, {6, -8}, {7}
    };
    for (auto& e : msgs)
        os::put(e);

    os::showTasks();

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("done %d ms\n", systick::millis());
}
