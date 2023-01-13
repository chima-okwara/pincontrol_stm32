#include "jee.h"

using namespace jeeh;

void (*consoleHandler)(char const*,int);

// called by first-time _write()
extern auto initConsole () -> void(*)(char const*,int);

uint32_t baudDiv (uint32_t mhz, uint32_t baud =115'200) {
    auto n = SystemCoreClock;
    while (n > mhz * 1'000'000)
        n /= 2;
    return n / baud;
}

#if STM32F103xB && BLUEPILL     // bluepill_f103c8
#define UART_V1     USART1
#define UART_EN     EN_USART1
#define UART_TX     "A9:1"
#elif STM32F103xB               // nucleo_f103rb
#define UART_V1     USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:1"
#elif STM32F103xE && PZ6806L    // PZ6806L
#define UART_V1     USART1
#define UART_EN     EN_USART1
#define UART_TX     "A9:1"
#elif STM32F302x8               // nucleo_f302r8
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:7"
#define UART_MHZ    36
#elif STM32F303x8               // nucleo_f303k8
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:7"
#define UART_MHZ    36
#elif STM32F303xE               // nucleo_f303ze
#define UART        USART3
#define UART_EN     EN_USART3
#define UART_TX     "D8:7"
#define UART_MHZ    36
#elif STM32F407xx               // disco_f407vg
#define UART_V1     USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:7"
#define UART_MHZ    42
#elif STM32F413xx               // nucleo_f413zh
#define UART_V1     USART3
#define UART_EN     EN_USART3
#define UART_TX     "D8:7"
#define UART_MHZ    25 // actually 50, but this works w/ the V1 code below
#elif STM32F722xx               // nucleo_f722ze
#define UART        USART3
#define UART_EN     EN_USART3
#define UART_TX     "D8:7"
#define UART_MHZ    108
#elif STM32F723xx               // disco_f723ie
#define UART        USART6
#define UART_EN     EN_USART6
#define UART_TX     "C6:8"
#define UART_MHZ    108
#elif STM32F750xx               // disco_f750n8
#define UART        USART1
#define UART_EN     EN_USART1
#define UART_TX     "A9:7"
#define UART_MHZ    108
#elif STM32G431xx               // nucleo_g431kb
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:7"
#elif STM32H743xx               // nucleo_h743zi
#define UART        USART3
#define UART_EN     EN_USART3
#define UART_TX     "D8:7"
#define UART_MHZ    120
#elif STM32H745xx               // nucleo_h745zi
#define UART        USART3
#define UART_EN     EN_USART3
#define UART_TX     "D8:7"
#define UART_MHZ    120
#elif STM32L011xx               // nucleo_l011k4
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:4"
#elif STM32L031xx               // nucleo_l031k6
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:4"
#elif STM32L053xx && JNZERO     // jeenode_zero
#define UART        USART1
#define UART_EN     EN_USART1
#define UART_TX     "A9:P4"
#elif STM32L053xx && DISCO      // disco_l053c8
#define UART        USART1
#define UART_EN     EN_USART1
#define UART_TX     "A9:P4"
#elif STM32L053xx               // nucleo_l053r8
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:4"
#elif STM32L073xx               // nucleo_l073rz
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:4"
#elif STM32L412xx               // nucleo_l412kb
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:U7"
#elif STM32L432xx               // nucleo_l432kc
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:7"
#elif STM32L496xx               // disco_l496ag
#define UART        USART2
#define UART_EN     EN_USART2
#define UART_TX     "A2:7"
#endif

#ifdef UART_V1
auto initConsole () -> void(*)(char const*,int) {
    enum { SR=0x00, DR=0x04, BRR=0x08, CR1=0x0C };

    Pin::config(UART_TX);
    RCC(UART_EN, 1) = 1;
#if UART_MHZ
    UART_V1[BRR] = baudDiv(UART_MHZ);
#else // STM32F103xB
    auto factor = SystemCoreClock <= 36'000'000 ? 2 : 1; // TODO reversed?
    UART_V1[BRR] = SystemCoreClock / 115200 / factor;
#endif
    UART_V1[CR1] = (1<<13) | (1<<3); // UE TE

    return [](char const* p, int n) {
        while (--n >= 0) {
            while (!UART_V1[SR](7)) {} // TXE
            UART_V1[DR] = *p++;
        }
    };
}
#endif

#ifdef UART // i.e. v2, the modern version
#ifndef UART_MHZ
#define UART_MHZ    (SystemCoreClock/1'000'000)
#endif

auto initConsole () -> void(*)(char const*,int) {
    enum { CR1=0x00, BRR=0x0C, ISR=0x1C, TDR=0x28 };

    Pin::config(UART_TX);
    RCC(UART_EN, 1) = 1;
    UART[BRR] = baudDiv(UART_MHZ);
    UART[CR1] = (1<<3) | (1<<0); // TE UE

    return [](char const* p, int n) {
        while (--n >= 0) {
            while (!UART[ISR](7)) {} // TXE
            UART[TDR] = *p++;
        }
    };
}
#endif

extern "C" int _write(int fd, char* ptr, int len) {
    if (consoleHandler == nullptr) {
        consoleHandler = initConsole();
        //msIdle(500);
    }
    if (fd < 1 || fd > 2 || consoleHandler == nullptr)
        return -1;

    consoleHandler(ptr, len);
    return len;
}
