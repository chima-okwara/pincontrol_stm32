#include <jee.h>

using namespace jeeh;

int main () {
    printf("%s: cycles @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    cycles::init();

    cycles::clear();
    for (volatile auto i = 0; i < 1000; ++i) {}
    printf("volatile up:      %6d cycles\n", cycles::count());

    cycles::clear();
    for (volatile auto i = 1000; --i > 0; ) {}
    printf("volatile down:    %6d cycles\n", cycles::count());

    cycles::clear();
    for (auto i = 0; i < 1000; ++i) asm ("nop");
    printf("inline asm nop:   %6d cycles\n", cycles::count());

    cycles::clear();
    for (auto i = 0; i < 1000; ++i) asm ("");
    printf("inline asm empty: %6d cycles\n", cycles::count());
}
