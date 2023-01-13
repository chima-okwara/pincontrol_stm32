#include <stm32l4xx.h>

void delay (int n) { for (auto i = 0; i < n * 1000; ++i) asm (""); }

int main () {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
    GPIOB->MODER &= ~GPIO_MODER_MODE3_Msk;
    GPIOB->MODER |= GPIO_MODER_MODE3_0;

    while (true) {
        GPIOB->ODR |= GPIO_ODR_OD3;
        delay(100);
        GPIOB->ODR &= ~GPIO_ODR_OD3_Msk;
        delay(400);
    }
}
