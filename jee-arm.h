// interface to all the ARM-specific definitions, see also jee-io.h

uint32_t fastClock (bool pll =true);
uint32_t slowClock (bool low =true);
void powerDown (bool standby =true);

extern void (*yield)();
extern void (*msIdle)(uint32_t ms);
void msBusy (uint32_t ms); // this default version busy-loops

[[noreturn]] void systemReset ();

void failAt (char const* f, int l);

void dumpHex (void const* p, int n =16, char const* msg =nullptr);

// from monty-1.3/lib/arch-stm32/arch.cpp - TODO turn into uint32_t const*
enum class Signature : uint32_t {
#if STM32F1
    cpu=0xE000'ED00, rom=0x1FFF'F7E0, pkg=0,           uid=0x1FFF'F7E8,
#elif STM32F4
    cpu=0xE004'2000, rom=0x1FFF'7A22, pkg=0,           uid=0x1FFF'7A10,
#elif STM32F723xx
    cpu=0xE004'2000, rom=0x1FF0'7A22, pkg=0x1FF0'7BF0, uid=0x1FF0'7A10,
#elif STM32F7
    cpu=0xE000'ED00, rom=0x1FF0'F442, pkg=0,           uid=0x1FF0'F420,
#elif STM32H7
    cpu=0xE000'ED00, rom=0x1FF1'E880, pkg=0,           uid=0x1FF1'E800,
#elif STM32L0
    cpu=0xE000'ED00, rom=0x1FF8'007C, pkg=0,           uid=0x1FF8'0050,
#elif STM32L4
    cpu=0xE000'ED00, rom=0x1FFF'75E0, pkg=0x1FFF'7500, uid=0x1FFF'7590,
#else
    cpu=0, rom=0, pkg=0, uid=0,
#endif
};

// this copies 32-bit words much faster than memcpy
void duffs (uint32_t* dst, uint32_t const* src, uint32_t count);

struct Pin {
    uint8_t id =0;

    constexpr Pin () =default;
    explicit constexpr Pin (char const* s) : id (parse(s)) {}

    constexpr int port () const { return id/16-1; }
    constexpr int pin () const { return id%16; }
    constexpr auto reg (int off) const { return GPIOA[0x400*port()+off]; }

    auto read () const { return reg(IDR)(pin()); }

    void write (int v) const {
        if constexpr (GPIOA.canBitBand())
            reg(ODR)(pin()) = v;
        else
            reg(BSRR) = ((1<<16) | (v&1)) << pin();
    }

    // shorthand
    void toggle () const { write(~reg(ODR)(pin())); }
    operator int () const { return read(); }
    void operator= (int v) const { write(v); }

    // pin definition string: [A-O][<pin#>]:[AFPO][DU][LNHV][<alt#>][,]
    // return -1 on error, 0 if no mode set, or the mode (always > 0)
    int init (char const* desc) {
        if (auto t = parse(desc); t != 0)
            id = t;
        if (id == 0)
            return -1;
        desc = strchr(desc, ':');
        return desc == nullptr ? 0 : mode(desc+1);
    }

    // reset pin to the default floating input mode
    void deinit () const { mode("F"); }

    // configure multiple pins, return nullptr if ok, else ptr to error
    static char const* config (char const* d, Pin* v =nullptr, int n =0);

    int mode (char const* desc) const;
    int mode (int m) const;

#if STM32F1
    enum { CRL=0x00, IDR=0x08, ODR=0x0C, BSRR=0x10 };
#else
    enum { MODER=0x00, IDR=0x10, ODR=0x14, BSRR=0x18 };
#endif
private:
    constexpr uint8_t parse (char const* s) {
        if (s == nullptr || *s < 'A' || *s >= 'P')
            return 0;
        uint8_t p = *s++ - '@', pnum = 0;
        while ('0' <= *s && *s <= '9')
            pnum = 10 * pnum + *s++ - '0';
        if (*s != 0 && *s != ':' && *s != ',')
            return 0;
        return 16*p + pnum;
    }
};

struct I2cGpio {
    Pin sda, scl; // pin definitions must be kept in this order
    uint16_t rate;

    void init (char const* desc, int r =20);

    void deinit () {
        Pin::config(":F,:U", &sda, 2); // keep SCL pulled up
    }

    void detect () const;

    bool start (uint8_t addr) const;
    void stop () const;

    int read (bool last) const;
    bool write (uint8_t data) const;

    bool readRegs (int addr, int reg, uint8_t* buf, int len) const;
    int readReg (int addr, int reg) const;
    bool readRegs16 (int addr, int reg, uint8_t* buf, int len) const;
    int readReg16 (int addr, int reg) const;
    bool writeReg (int addr, int reg, int val) const;
    bool writeReg16 (int addr, int reg, int val) const;

private:
    void hold () const {
        for (volatile int i = rate; --i >= 0; ) {}
    }
    void sclLo () const {
        hold(); scl = 0;
    }
    void sclHi () const {
        hold(); scl = 1; for (auto i = 10'000; scl == 0 && i >= 0; --i) {}
    }
};

struct SpiBase {
    Pin mosi, miso, sclk, nsel; // pin definitions must be kept in this order

    void enable () const { nsel = 0; }
    void disable () const { nsel = 1; }

    virtual int xfer (int v) =0;
    virtual int block (uint8_t const* out, uint8_t* in, int len) =0;
};

struct SpiSoft final : SpiBase {
    uint16_t rate =0;
    uint8_t cpol =0;

    void init (char const* desc) {
        Pin::config(desc, &mosi, 4);
        nsel.mode("U"); disable(); // start with NSEL high
        Pin::config(":PH,:UH,:PH,PH", &mosi, 4);
        sclk = cpol;
    }

    void deinit () {
        Pin::config(":F,,,:U", &mosi, 4); // keep NSEL pulled up
    }

    int xfer (int v) override {
        auto r = 0;
        for (auto i = 0; i < 8; ++i) {
            mosi = v >> 7;
            v <<= 1;
            spin(rate);
            sclk = ~cpol;
            spin(rate);
            r = (r<<1) | miso;
            sclk = cpol;
        }
        return r;
    }

    int block (uint8_t const* out, uint8_t* in, int len) override {
        int b = 0;
        for (auto i = 0; i < len; ++i) {
            b = xfer(out != nullptr ? out[i] : 0);
            if (in != nullptr)
                in[i] = b;
        }
        return b;
    }

//private:
    void spin (volatile int i) const {
        while (--i >= 0) {}
    }
};

namespace watchdog {
    int resetCause (); // watchdog: -1, nrst: 1, power: 2, other: 0

    void init (int rate =6);   // max timeout, 0 ≈ 500 ms, 6 ≈ 32 s
    void reload (int n =4095); // 0..4095 x 125 µs (0) .. 8 ms (6)
    void kick ();
}

namespace cycles {
    constexpr IoReg<0xE000'1000> DWT;

    void init ();
    void deinit ();

    inline void clear () { DWT[0x04] = 0; }
    inline uint32_t count () { return DWT[0x04]; }

    uint32_t millis ();
    uint32_t micros ();

    void msBusy (uint32_t ms);
}

namespace systick {
    void init (uint8_t ms =100, void (*fun)(uint32_t) =nullptr);
    void deinit ();

    uint32_t millis ();
    uint32_t micros ();

    void msBusy (uint32_t ms);
}

namespace rtc {
    struct DateTime { uint8_t yr, mo, dy, hh, mm, ss; };

    void init ();
    DateTime get ();
    void set (DateTime dt);

    uint32_t getData (int reg);
    void setData (int reg, uint32_t val);
}

struct BlockIRQ {
    uint32_t mask;

    BlockIRQ () { asm volatile ("mrs %0, primask; cpsid i" : "=r" (mask)); }
    ~BlockIRQ () { asm volatile ("msr primask, %0" :: "r" (mask)); }
};
