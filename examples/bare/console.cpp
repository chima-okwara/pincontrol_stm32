#include <jee.h>
#include <cassert>

using namespace jeeh;

int main () {
    printf("%s: console @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    assert(true);
    assert(1 == 2);
    assert(false); // not reached
}
