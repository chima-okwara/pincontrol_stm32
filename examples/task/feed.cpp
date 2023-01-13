// send out a continous feed of small uart packets, used to measure hiccups

#include <jee.h>
using namespace jeeh;
#include "board.h"

constexpr Pin led (LED_PIN);

void feed (Message&, void*) {
    auto& msg = os::get();
    os::delay(5);

    char buffer [72];
    memset(buffer, ' ', sizeof buffer);

    auto chunk = msg.mTag;
    auto num = sizeof buffer / chunk;
    auto lines = 50;
    verify(sizeof buffer % chunk == 0);

    Message msgs [num] {};

    for (auto i = 0U; i < num; ++i) {
        auto p = buffer + i * chunk;
        if (chunk > 1)
            p[chunk-2] = i + 'A';
        p[chunk-1] = i < num-1 ? '.' : '\n';

        auto& m = msgs[i];
        m.mDst = -2;
        m.mTag = 'W';
        m.mLen = chunk;
        m.mPtr = p;
        os::put(m);
    };

    auto t = systick::millis();

    for (auto i = 0U; i < num * (lines-1); ++i) {
        auto& msg = os::get();
        verify(msg.mTag == 'W');

        led = msg.mPtr == buffer;

        verify(msg.mDst == -2);
        os::put(msg);
    }

    // done, eat up all remaining replies
    for (auto i = 0U; i < num; ++i)
        os::get();

    t = systick::millis() - t;

    printf("sent %d lines of %d bytes, in %d chunks of %d bytes\n",
            lines, sizeof buffer, num, chunk);

    printf("i.e. %d bytes / %d ms = %d baud\n",
            lines * sizeof buffer, t, (lines * sizeof buffer * 10 * 1000) / t);
}

int main () {
    //fastClock();
    printf("\n### %s: feed @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    uint32_t stack [250];
    Tasker tasker (stack);

    Uart uart;
    verify(~uart.devId == -2);
    uart.init(UART_CONSOLE);

    uint32_t feedStack [400]; // needs room for buffer[] and msgs[]

    // repeat the test loop for various chunk sizes
    int8_t const sizes [] = { 12, 6, 4, 3, 2, 1 };
    for (auto e : sizes) {
        os::fork(feedStack, feed);

        Message msg {1, e};
        os::put(msg);

        printf("idling @ %d\n", systick::millis());
        while (tasker.count > 0)
            os::get();
    }

    printf("done %d ms\n", systick::millis());
}
