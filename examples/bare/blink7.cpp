#include <jee.h>

using namespace jeeh;

Pin leds [7];

int main () {
    systick::init();
    Pin::config("B3:P,A0,A1,A3,A4,A5,A6", leds, sizeof leds);

    for (int n = 0; ; ++n) {
        for (auto i = 0U; i < sizeof leds; ++i)
            leds[i] = (n>>i) & 1;
        msIdle(100);
    }
}
