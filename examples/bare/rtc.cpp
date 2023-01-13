#include <jee.h>

using namespace jeeh;

int main () {
    printf("%s: rtc @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    rtc::init();

    auto t = rtc::get();
    auto n = rtc::getData(0);

    printf("%02d/%02d/%02d %02d:%02d:%02d #%d\n",
            t.yr, t.mo, t.dy, t.hh, t.mm, t.ss, n);

    rtc::setData(0, n + 1);

    systemReset();
}
