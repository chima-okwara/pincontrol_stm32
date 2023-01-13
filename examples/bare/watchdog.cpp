#include <jee.h>

using namespace jeeh;

int main () {
    printf("%s: watchdog @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    printf("reset case %d\n", watchdog::resetCause());

    watchdog::init(1); // 2 sec
    watchdog::kick();
}
