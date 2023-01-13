#include <jee.h>

using namespace jeeh;

int main () {
    printf("%s: sysreset @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    systemReset();
}
