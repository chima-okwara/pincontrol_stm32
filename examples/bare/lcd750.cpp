#include <jee.h>
#include <jee/lcd-f750.h>

using namespace jeeh;

int main () {
    fastClock();
    printf("%s: lcd750 @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    systick::init(1);

    lcd750::FrameBuffer<1> bg;
    lcd750::FrameBuffer<2> fg;

    lcd750::init();
    bg.init();
    fg.init();

    for (int y = 0; y < lcd750::HEIGHT; ++y)
        for (int x = 0; x < lcd750::WIDTH; ++x)
            bg(x, y) = x ^ y;

    for (int y = 0; y < lcd750::HEIGHT; ++y)
        for (int x = 0; x < lcd750::WIDTH; ++x)
            fg(x, y) = ((16*x)/lcd750::WIDTH << 4) | (y >> 4);

    printf("done %d\n", systick::millis());
}
