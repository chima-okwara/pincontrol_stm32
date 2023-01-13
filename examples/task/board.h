// some board-specific definitions to simplify different builds

#if STM32F413xx // F413ZH Nucleo-144
#include <jee/uart-f1347h7.h>
#define LED_PIN         "B0"
#define UART_CONSOLE    UART3_CONFIG, "D8:7,D9"

// TODO APB1 bus has to use 25 MHz iso 50 MHz to get the proper baudrate
#define UART1_CONFIG { USART1.ADDR, EN_USART1, 50, Irq::USART1, \
            Irq::DMA2_Stream2, Irq::DMA2_Stream7, 1, 2, 7, 4, 4 } // "D8:7,D9"
#define UART2_CONFIG { USART2.ADDR, EN_USART2, 25, Irq::USART2, \
            Irq::DMA1_Stream5, Irq::DMA1_Stream6, 0, 5, 6, 4, 4 } // "A9:7,A10"
#define UART3_CONFIG { USART3.ADDR, EN_USART3, 25, Irq::USART3, \
            Irq::DMA1_Stream1, Irq::DMA1_Stream3, 0, 1, 3, 4, 4 } // "A2:7,A3"
#define UART4_CONFIG { UART4.ADDR,  EN_UART4,  25, Irq::USART4, \
            Irq::DMA1_Stream2, Irq::DMA1_Stream4, 0, 2, 4, 4, 4 } // "A0:8,A1"
#define UART5_CONFIG { UART5.ADDR,  EN_UART5,  25, Irq::UART5, \
            Irq::DMA1_Stream0, Irq::DMA1_Stream7, 0, 0, 7, 4, 8 } // "C12:8,D2"
#define UART6_CONFIG { USART6.ADDR, EN_USART6, 50, Irq::USART6, \
            Irq::DMA2_Stream1, Irq::DMA2_Stream6, 1, 1, 6, 5, 5 } // "C6:8,C7"
#define UART10_CONFIG { UART10.ADDR, EN_UART10, 50, Irq::UART10, \
            Irq::DMA2_Stream3, Irq::DMA2_Stream5, 1, 3, 5, 9, 9 } // "E3:11,E2"

#elif STM32F723xx // F723IE Discovery
#include <jee/uart-f1347h7.h>
#define LED_PIN         "B1"
#define UART_CONSOLE    UART6_CONFIG, "C6:8,C7"

#define UART2_CONFIG { USART2.ADDR, EN_USART2, 108, Irq::USART2, \
            Irq::DMA1_Stream5, Irq::DMA1_Stream6, 0, 5, 6, 4, 4 } // "A2:7,A3"
#define UART6_CONFIG { USART6.ADDR, EN_USART6, 108, Irq::USART6, \
            Irq::DMA2_Stream2, Irq::DMA2_Stream6, 1, 2, 6, 5, 5 } // "C6:8,C7"

#elif STM32F750xx // F750N8 Discovery
#include <jee/uart-f1347h7.h>
#include <jee/eth-f7.h>
#define LED_PIN         "I1"
#define UART_CONSOLE    UART1_CONFIG, "A9:7,B7"

#define UART1_CONFIG { USART1.ADDR, EN_USART1, 108, Irq::USART1, \
            Irq::DMA2_Stream5, Irq::DMA2_Stream7, 1, 5, 7, 4, 4 } // "A9:7,B7"

#elif STM32L053xx // L053C8 Discovery
#define LED_PIN         "A5"

#elif STM32L432xx // L432KC Nucleo-32
#include <jee/uart-l04.h>
#define LED_PIN         "B3"
#define UART_CONSOLE    UART2_CONFIG, "A2:7,A15:3"

#define UART1_CONFIG { USART1.ADDR, EN_USART1, 80, Irq::USART1, \
            Irq::DMA1_CH5, Irq::DMA1_CH4, 0, 5-1, 4-1, 2, 2 } // "A9:7,A10"
#define UART2_CONFIG { USART2.ADDR, EN_USART2, 80, Irq::USART2, \
            Irq::DMA1_CH6, Irq::DMA1_CH7, 0, 6-1, 7-1, 2, 2 } // "A2:7,A15:3"

#endif
