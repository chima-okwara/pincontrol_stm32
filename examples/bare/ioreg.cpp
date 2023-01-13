#include <jee.h>

using namespace jeeh;

void delay (int n) { for (auto i = 0; i < n * 1000; ++i) asm (""); }

int main () {
    enum { MODER=0x00, ODR=0x14 };

    RCC[AHB2ENR](1) = 1;
    GPIOB[MODER](6, 2) = 1;

    while (true) {
        GPIOB[ODR](3) = 1;
        delay(100);
        GPIOB[ODR](3) = 0;
        delay(400);
    }
}
