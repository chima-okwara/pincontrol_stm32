#include <jee.h>

using namespace jeeh;

#if STM32F103xB && BLUEPILL // bluepill
constexpr Pin led ("C13");
#elif STM32F103xB // F103RB Nucleo-64
constexpr Pin led ("A5");
#elif STM32F103xE && PZ6806L // PZ6806L
constexpr Pin led ("C0");
#elif STM32F302x8 // F302R8 Nucleo-32
constexpr Pin led ("B13");
#elif STM32F303x8 // F303K8 Nucleo-32
constexpr Pin led ("B3");
#elif STM32F303xE // F303ZE Nucleo-144
constexpr Pin led ("B0");
#elif STM32F407xx // F407VG Discovery
constexpr Pin led ("D12");
#elif STM32F413xx // F413ZH Nucleo-144
constexpr Pin led ("B0");
#elif STM32F722xx // F722ZE Nucleo-144
constexpr Pin led ("B0");
#elif STM32F723xx // F723IE Discovery
constexpr Pin led ("B1");
#elif STM32F750xx // F750N8 Discovery
constexpr Pin led ("I1");
#elif STM32G431xx // G431KB Nucleo-32
constexpr Pin led ("B8");
#elif STM32H743xx // H743ZI Nucleo-144
constexpr Pin led ("B0");
#elif STM32H745xx // H745ZI Nucleo-144
constexpr Pin led ("B0");
#elif STM32L011xx // L011K4 Nucleo-32
constexpr Pin led ("B3");
#elif STM32L031xx // L031K6 Nucleo-32
constexpr Pin led ("B3");
#elif STM32L053xx && JNZERO // jeenode-zero
constexpr Pin led ("A8");
#elif STM32L053xx && DISCO // L053R8 Discovery
constexpr Pin led ("A5");
#elif STM32L053xx // L053R8 Nucleo-64
constexpr Pin led ("A5");
#elif STM32L073xx // L073RZ Nucleo-64
constexpr Pin led ("A5");
#elif STM32L412xx // L412KC Nucleo-32
constexpr Pin led ("B3");
#elif STM32L432xx // L432KC Nucleo-32
constexpr Pin led ("B3");
#elif STM32L496xx // L496AG Discovery
constexpr Pin led ("A5");
#endif

int main () {
#if !STM32G4 && !STM32H7 // TODO
    fastClock();
#endif
    led.mode("P");
#if BLUEPILL | JNZERO | PZ6806L
    led = 1; // inverted logic
#endif
    led.toggle();
    printf("%s: any @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.toggle();

    systick::init();
    auto t = systick::micros();
    msIdle(100);
    printf("msIdle(100) = %d µs\n", systick::micros() - t);

    while (true) {
        led.toggle();
        msIdle(100);
        led.toggle();
        msIdle(400);
    }
}
