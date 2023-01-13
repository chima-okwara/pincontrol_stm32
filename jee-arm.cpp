#include "jee.h"

#if STM32F723xx | STM32F750xx // FIXME hard-coded hack ...
#define XTAL 25
#endif

#ifndef XTAL
#define XTAL 8
#endif

namespace jeeh {

void (*yield)() = []() { asm ("wfi"); };
void (*msIdle)(uint32_t ms) = msBusy;

// these dividers were determined empirically, see examples/src/busyloop.cpp
// it depends on µC arch, clock rate, flash wait, compiler flags, and loop code
void msBusy (uint32_t ms) {
#if STM32F1
    uint32_t divider = 8000;
#elif STM32F3
    uint32_t divider = 8000;
#elif STM32F4
    uint32_t divider = 4000;
#elif STM32F7
    uint32_t divider = 2000;
#elif STM32G4
    uint32_t divider = 4000;
#elif STM32H7
    uint32_t divider = 2000;
#elif STM32L0
    uint32_t divider = 5000; // no DWT
#elif STM32L4
    uint32_t divider = 4000;
#endif
    auto n = ms * (SystemCoreClock / divider);
    while (--n > 0) asm ("");
}

void duffs (uint32_t* dst, uint32_t const* src, uint32_t count) {
    // see https://en.wikipedia.org/wiki/Duff%27s_device
    auto n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do { *dst++ = *src++; [[fallthrough]];
        case 7:      *dst++ = *src++; [[fallthrough]];
        case 6:      *dst++ = *src++; [[fallthrough]];
        case 5:      *dst++ = *src++; [[fallthrough]];
        case 4:      *dst++ = *src++; [[fallthrough]];
        case 3:      *dst++ = *src++; [[fallthrough]];
        case 2:      *dst++ = *src++; [[fallthrough]];
        case 1:      *dst++ = *src++;
                } while (--n > 0);
    }
}

void systemReset () {
    volatile auto n = SystemCoreClock >> 15;
    while (n > 0) --n; // brief delay to let uart TX finish, etc
    asm volatile ("dsb");
    SCB[0x0C] = (0x5FA<<16) | (1<<2); // SCB AIRCR reset
    asm volatile ("dsb");
    while (true) {}
}

void dumpHex (void const* p, int n, char const* msg) {
    if (msg != nullptr)
        printf("%s: (%db)\n", msg, n);
    auto q = (uint8_t const*) p;
    for (int off = 0; off < n; off += 16) {
        printf(" %03x:", off);
        for (int i = 0; i < 16; ++i) {
            if (i % 4 == 0)
                printf(" ");
            if (off+i >= n)
                printf("..");
            else
                printf("%02x", q[off+i]);
        }
        for (int i = 0; i < 16; ++i) {
            if (i % 4 == 0)
                printf(" ");
            auto b = q[off+i];
            printf("%c", off+i >= n ? ' ' : ' ' <= b && b <= '~' ? b : '.');
        }
        printf("\n");
    }
}

char const* Pin::config (char const* d, Pin* v, int n) {
    Pin dummy;
    int lastMode = 0;
    while (*d != 0) {
        if (--n < 0)
            v = &dummy;
        auto m = v->init(d);
        if (m < 0)
            break;
        if (m != 0)
            lastMode = m;
        else if (lastMode != 0)
            v->mode(lastMode);
        auto p = strchr(d, ',');
        if (p == nullptr)
            return n > 0 ? d : nullptr;
        d = p+1;
        ++v;
    }
    return d;
}

int Pin::mode (char const* desc) const {
    int m = 0, a = 0;
    for (auto s = desc; *s != ',' && *s != 0; ++s)
        switch (*s) {     // 1 pp ss t mm
            case 'A': m  = 0b1'00'00'0'11; break; // m=11 analog
            case 'F': m  = 0b1'00'00'0'00; break; // m=00 float
            case 'D': m |= 0b1'10'00'0'00; break; // m=00 pull-down
            case 'U': m |= 0b1'01'00'0'00; break; // m=00 pull-up

            case 'P': m  = 0b1'00'01'0'01; break; // m=01 push-pull
            case 'O': m  = 0b1'00'01'1'01; break; // m=01 open drain

            case 'L': m &= 0b1'11'00'1'11; break; // s=00 low speed
            case 'N':                      break; // s=01 normal speed
            case 'H': m ^= 0b0'00'11'0'00; break; // s=10 high speed
            case 'V': m |= 0b0'00'10'0'00; break; // s=11 very high speed

            default:  if (*s < '0' || *s > '9' || a > 1) return -1;
                        m = (m & ~0b11) | 0b10;     // m=10 alt mode
                        a = 10 * a + *s - '0';
            case ',': break; // valid as terminator
        }
    return mode(m + (a<<8));
}

int Pin::mode (int m) const {
#if STM32F1
    RCC(EN_IOPA+port(), 1) = 1;
    RCC(EN_AFIO, 1) = 1;
    // messy code to keep the mode encoding the same as other families
    auto cr = 0, mm = m&3, t = (m>>2)&1, ss = (m>>3)&3, pp = (m>>5)&1;
    switch (mm) {
        case 0b00: // float or pull-up/-down
            if (pp) {
                cr = 0b10'00; // input with pull-up/-down
                reg(ODR)(pin()) = pp & 1;
            } else
                cr = 0b01'00; // float
            break;
        case 0b01: // output
        case 0b10: // alternate
            cr = ((mm&0b10) | t) << 2;
            cr |= ((0b11'11'01'10) >> (2*ss)) & 3; // low, normal, high, high
            break;
        case 0b11: // analog
            break;
    }
    reg(CRL)(4*pin(), 4) = cr; // CRL/CRH
#else
    enum { TYPER=0x04, OSPEEDR=0x08, PUPDR=0x0C, AFRL=0x20, AFRH=0x24 };

#if STM32F3
    auto EN_GPIOA = 17 + 8 * 0x14; // IOPENR
#elif STM32L0
    auto EN_GPIOA = 8 * 0x2C; // IOPENR
#endif
    RCC(EN_GPIOA + port(), 1) = 1;

    auto p = pin();
    reg(MODER)  (2*p,2) = m;
    reg(TYPER)  (p)     = m >> 2;
    reg(OSPEEDR)(2*p,2) = m >> 3;
    reg(PUPDR)  (2*p,2) = m >> 5;
    reg(AFRL)   (4*p,4) = m >> 8;
#endif // STM32F1
    return m;
}
void I2cGpio::init (char const* desc, int r) {
    Pin::config(desc, &sda, 2);
    Pin::config(":OU,:OU", &sda, 2);
    sda = 1;
    scl = 1;
    rate = r;
}

void I2cGpio::detect () const {
    for (auto i = 0; i < 128; i += 16) {
        printf("%02x:", i);
        for (auto j = 0; j < 16; ++j) {
            int addr = i + j;
            if (0x08 <= addr && addr <= 0x77) {
                bool ack = start(2*addr);
                stop();
                printf(ack ? " %02x" : " --", addr);
            } else
                printf("   ");
        }
        printf("\n");
    }
}

bool I2cGpio::start (uint8_t addr) const {
    sclLo();
    sclHi();
    sda = 0;
    return write(addr);
}

void I2cGpio::stop () const {
    sda = 0;
    sclHi();
    sda = 1;
    hold();
}

int I2cGpio::read (bool last) const {
    uint8_t data = 0;
    for (auto mask = 0x80; mask != 0; mask >>= 1) {
        sclHi();
        if (sda)
            data |= mask;
        sclLo();
    }
    sda = last;
    sclHi();
    sclLo();
    if (last)
        stop();
    sda = 1;
    return data;
}

bool I2cGpio::write (uint8_t data) const {
    sclLo();
    for (auto mask = 0x80; mask != 0; mask >>= 1) {
        sda = (data & mask) != 0;
        sclHi();
        sclLo();
    }
    sda = 1;
    sclHi();
    hold();
    bool ack = !sda;
    sclLo();
    return ack;
}

bool I2cGpio::readRegs (int addr, int reg, uint8_t* buf, int len) const {
    start(2*addr);
    if (!write(reg)) {
        stop();
        return false;
    }
    start(2*addr+1);
    for (auto i = 0; i < len; ++i)
        *buf++ = read(i == len-1);
    return true;
}

int I2cGpio::readReg (int addr, int reg) const {
    uint8_t val;
    if (!readRegs(addr, reg, &val, sizeof val))
        return -1;
    return val;
}

bool I2cGpio::readRegs16 (int addr, int reg, uint8_t* buf, int len) const {
    start(2*addr);
    auto ack = write(reg>>8);
    if (ack)
        ack = write(reg);
    if (!ack) {
        stop();
        return false;
    }
    start(2*addr+1);
    for (auto i = 0; i < len; ++i)
        *buf++ = read(i == len-1);
    return true;
}

int I2cGpio::readReg16 (int addr, int reg) const {
    uint8_t val [2];
    if (!readRegs16(addr, reg, val, sizeof val))
        return -1;
    return (val[0]<<8) | val[1];
}

bool I2cGpio::writeReg (int addr, int reg, int val) const {
    start(2*addr);
    auto ack = write(reg);
    if (ack)
        ack = write(val);
    stop();
    return ack;
}

bool I2cGpio::writeReg16 (int addr, int reg, int val) const {
    start(2*addr);
    auto ack = write(reg>>8);
    if (ack)
        ack = write(reg);
    if (ack)
        ack = write(val>>8);
    if (ack)
        ack = write(val);
    stop();
    return ack;
}

namespace watchdog {
    enum { KR=0x00, PR=0x04, RLR=0x08, SR=0x0C };

    uint32_t cause; // "semi public"

    int resetCause () {
#if STM32F1 | STM32F3
        enum { CSR=0x24, RMVF=24 };
#elif STM32F4 | STM32F7
        enum { CSR=0x74, RMVF=24 };
#elif STM32H7
        enum { CSR=0xD0, RMVF=16 };
#elif STM32G4 | STM32L0 | STM32L4
        enum { CSR=0x94, RMVF=23 };
#endif
        if (cause == 0) {
            cause = RCC[CSR];
            RCC[CSR](RMVF) = 1; // clears all reset-cause flags
        }
#if STM32H7
        return cause & (1<<26) ? -1 :     // iwdg
               cause & (5<<21) ? 2 :      // por/bor
               cause & (1<<17) ? 1 : 0;   // nrst, or other
#else
        return cause & (1<<29) ? -1 :     // iwdg
               cause & (1<<27) ? 2 :      // por/bor
               cause & (1<<26) ? 1 : 0;   // nrst, or other
#endif
    }

#if STM32H745xx
#define IWDG IWDG1
#endif

    void init (int rate) {
        while (IWDG[SR](0)) {}  // wait until !PVU
        IWDG[KR] = 0x5555;      // unlock PR
        IWDG[PR] = rate;        // max timeout, 0 = 400ms, 7 = 26s
        IWDG[KR] = 0xCCCC;      // start watchdog
    }

    void reload (int n) {
        while (IWDG[SR](0)) {}  // wait until !PVU
        IWDG[KR] = 0x5555;      // unlock PR
        IWDG[RLR] = n;
        kick();
    }

    void kick () {
        IWDG[KR] = 0xAAAA;      // reset the watchdog timout
    }
}

namespace cycles {
    enum { CTRL=0x000, CYCCNT=0x004, LAR=0xFB0, DEMCR=0x0FC };

    void init () {
        SCB[DEMCR](24) = 1; // TRCENA
        DWT[LAR] = 0xC5ACCE55;
        clear();
        DWT[CTRL](0) = 1;
    }

    void deinit () {
        DWT[CTRL](0) = 0;
    }

    uint32_t millis () {
        return count() / (SystemCoreClock/1000);
    }

    uint32_t micros () {
        // scaled to work with any clock rate multiple of 100 kHz
        return (10*count()) / (SystemCoreClock/100'000);
    }

    void msBusy (uint32_t ms) {
        auto n = ms * (SystemCoreClock/1000);
        auto t = count();
        while (count() - t < n) {}
    }
}

namespace systick {
    volatile uint32_t ticks;
    uint8_t rate;
    void (*ticker)(uint32_t) = [](uint32_t) {};

    void init (uint8_t ms, void (*fun)(uint32_t)) {
        deinit();
        if (fun != nullptr)
            ticker = fun;
        rate = ms;
        STK[0x04] = (ms*(SystemCoreClock/1000))/8-1; // reload value
//printf("systick %d %d %d\n", (ms*(SystemCoreClock/1000))/8-1, SystemCoreClock/1000, ms);
        STK[0x08] = 0;                               // current
        STK[0x00] = 0b011;                           // control, ÷8 mode
    }

    void deinit () {
        ticks = millis();
        STK[0x00] = 0;
        rate = 0;
    }

    uint32_t millis () {
        while (true) {
            uint32_t t = ticks, n = STK[0x08];
            if (t == ticks)
                return t - (n*8)/(SystemCoreClock/1000);
        } // ticked just now, spin one more time
    }

    uint32_t micros () {
        // scaled to work with any clock rate multiple of 100 kHz
        while (true) {
            uint32_t t = ticks, n = STK[0x08];
            if (t == ticks)
                return (t%1000)*1000 - (n*80)/(SystemCoreClock/100'000);
        } // ticked just now, spin one more time
    }

    void msBusy (uint32_t ms) {
        auto t = millis();
        while (millis() - t < ms)
            yield(); // granularity will depend on init's rate setting
    }

    extern "C" void SysTick_Handler () { ticker(ticks += rate); }
}

#if STM32F4 | STM32F7 | STM32L4
namespace rtc {
    enum { TR=0x00,DR=0x04,CR=0x08,ISR=0x0C,WPR=0x24,BKPR=0x50 };
#if STM32F4 | STM32F7
    enum { BDCR=0x70 };
#elif STM32L4
    enum { BDCR=0x90 };
#endif

    void init () {
        RCC(EN_PWR, 1) = 1;
        PWR[0x00](8) = 1; // DBP

        RCC[BDCR](0) = 1;             // LSEON backup domain
        while (RCC[BDCR](1) == 0) {}  // wait for LSERDY
        RCC[BDCR](8) = 1;             // RTSEL = LSE
        RCC[BDCR](15) = 1;            // RTCEN
    }

    DateTime get () {
        RTC[WPR] = 0xCA;  // disable write protection, [1] p.803
        RTC[WPR] = 0x53;

        RTC[ISR](5) = 0;              // clear RSF
        while (RTC[ISR](5) == 0) {}   // wait for RSF

        RTC[WPR] = 0xFF;  // re-enable write protection

        // shadow registers are now valid and won't change while being read
        uint32_t tod = RTC[TR];
        uint32_t doy = RTC[DR];

        DateTime dt;
        dt.ss = (tod & 0xF) + 10 * ((tod>>4) & 0x7);
        dt.mm = ((tod>>8) & 0xF) + 10 * ((tod>>12) & 0x7);
        dt.hh = ((tod>>16) & 0xF) + 10 * ((tod>>20) & 0x3);
        dt.dy = (doy & 0xF) + 10 * ((doy>>4) & 0x3);
        dt.mo = ((doy>>8) & 0xF) + 10 * ((doy>>12) & 0x1);
        // works until end 2063, will fail (i.e. roll over) in 2064 !
        dt.yr = ((doy>>16) & 0xF) + 10 * ((doy>>20) & 0x7);
        return dt;
    }

    void set (DateTime dt) {
        RTC[WPR] = 0xCA;  // disable write protection, [1] p.803
        RTC[WPR] = 0x53;

        RTC[ISR](7) = 1;             // set INIT
        while (RTC[ISR](6) == 0) {}  // wait for INITF
        RTC[TR] = (dt.ss + 6 * (dt.ss/10)) |
            ((dt.mm + 6 * (dt.mm/10)) << 8) |
            ((dt.hh + 6 * (dt.hh/10)) << 16);
        RTC[DR] = (dt.dy + 6 * (dt.dy/10)) |
            ((dt.mo + 6 * (dt.mo/10)) << 8) |
            ((dt.yr + 6 * (dt.yr/10)) << 16);
        RTC[ISR](7) = 0;             // clear INIT

        RTC[WPR] = 0xFF;  // re-enable write protection
    }

    uint32_t getData (int reg) {
        return RTC[BKPR+4*reg];  // regs 0..31
    }

    void setData (int reg, uint32_t val) {
        RTC[BKPR+4*reg] = val;  // regs 0..31
    }
}
#endif

#if STM32F1
#include "arch/stm32f1.h"
#elif STM32F3
#include "arch/stm32f3.h"
#elif STM32F4
#include "arch/stm32f4.h"
#elif STM32F7
#include "arch/stm32f7.h"
#elif STM32H7
#include "arch/stm32h7.h"
#elif STM32L0
#include "arch/stm32l0.h"
#elif STM32L4
#include "arch/stm32l4.h"
#endif // STM32L4

#if !STM32H7 // TODO look into STM32H7 ...

void powerDown (bool standby) {
    RCC(EN_PWR, 1) = 1;
#if STM32F1 | STM32F3 | STM32F4 | STM32F7
    PWR[0x00](1) = standby; // PDDS if standby
#elif STM32G4
    // set to either shutdown or stop 1 mode
    PWR[0x00] = (0b01<<9) | (standby ? 0b100 : 0b001);
    PWR[0x18] = 0b1'1111; // clear CWUFx
#elif STM32L0
    // LDO range 2, FWU, ULP, DBP, CWUF, PDDS (if standby), LPSDSR
    PWR[0x00] = (0b10<<11) | (1<<10) | (1<<9) | (1<<8) | (1<<2) |
                            ((standby ? 1 : 0)<<1) | (1<<0);
#elif STM32L4
    // set to either shutdown or stop 1 mode
    PWR[0x00] = (0b01<<9) | (standby ? 0b100 : 0b001);
    PWR[0x18] = 0b1'1111; // clear CWUFx
#else
#warning unknown chip type
#endif
    SCB[0x10](2) = 1; // set SLEEPDEEP
    asm ("wfe");
}

#endif // !STM32H7

} // namespace jeeh
