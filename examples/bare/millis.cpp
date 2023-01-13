#include <jee.h>

using namespace jeeh;

int main () {
    printf("%s: millis @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    for (auto e : (int[]){ 1, 10, 100 }) {
        systick::init(e);

        for (auto i = 0; i < 3; ++i) {
            asm ("wfi");
            printf("%3d ms %6d µs\n", systick::millis(), systick::micros());
        }
    }
}
