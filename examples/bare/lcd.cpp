#include <jee.h>
#include <jee/lcd-st7789.h>

using namespace jeeh;

ST7789 lcd;

auto lcdAddr = (uint16_t*) 0x6400'0000;

void ST7789::cmd (int v) {
    lcdAddr[0] = v;
    asm ("dmb");
}

void ST7789::dat (int v) {
    lcdAddr[1] = v;
    asm ("dmb");
}

void initFsmcPins () {
    RCC(EN_FMC, 1) = 1;

    Pin::config("D0:V12,D1,D4,D5,D7,D8,D9,D10,D11,D12,D14,D15,"
                "E0,E1,E7,E8,E9,E10,E11,E12,E13,E14,E15,"
                "F0,F1,F2,F3,F4,F5,F12,F13,F14,F15,"
                "G0,G1,G2,G3,G4,G5,G9");
}

void initLcd () {
    enum { BCR2=0x08, BTR2=0x0C };

    FMC[BCR2] = (1<<12) | (1<<7) | (1<<4);
    FMC[BTR2] = (1<<20) | (6<<8) | (2<<4) | (9<<0);
    FMC[BCR2](0) = 1; // MBKEN

    struct { Pin light, reset; } pins;
    Pin::config("H11:P,H7", &pins.light, sizeof pins);

    msIdle(5); pins.reset = 1; msIdle(5);

    lcd.init();
    lcd.clear();

    pins.light = 1; // on
}

int main () {
    fastClock();
    printf("%s: lcd @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    initFsmcPins();
    initLcd();

    cycles::init();
    systick::init();

    {
        auto t = systick::micros();
        cycles::clear();

        lcd.clear();

        auto n = cycles::count();
        printf("clear: %7d cycles, %5d µs\n", n, systick::micros() - t);
    }

    {
        auto t = systick::micros();
        cycles::clear();

        lcd.fill(50, 10, 100, 100, 0xF800); // red
        lcd.fill(20, 200, 200, 10, 0x07E0); // green
        lcd.fill(200, 60, 30, 100, 0x001F); // blue

        auto n = cycles::count();
        printf("fills: %7d cycles, %5d µs\n", n, systick::micros() - t);
    }
}
