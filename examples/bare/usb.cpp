#include <jee.h>

using namespace jeeh;

#if STM32F407xx // F407VG Discovery
constexpr Pin led ("D12");
#elif STM32F723xx // F723IE Discovery
constexpr Pin led ("B1");
#endif

namespace usb {
    constexpr auto USB = OTG_FS_GLOBAL; // shorthand

    enum {
        GAHBCFG=0x008, GUSBCFG=0x00C, GINTSTS=0x014, GRXSTSP=0x020,
        GRXFSIZ=0x024, DIEPTXF0=0x028, GCCFG=0x038, DIEPTXF1=0x104,
        DCFG=0x800, DSTS=0x808, DIEPCTL0=0x900, DIEPTSIZ0=0x910,
        DTXFSTS0=0x918, DOEPCTL0=0xB00, DOEPTSIZ0=0xB10,
    };

    struct { uint8_t typ, req; uint16_t val, idx, len; } setupPkt;

    uint32_t inData;
    uint8_t inPending, inReady;
    bool dtr; // only true when there is an active serial session

    uint8_t const devDesc [] = {
        18, 1, 0, 2, 2, 0, 0, 64,
        0x83, 0x04, // vendor
        0x40, 0x57, // product
        0, 2, 0, 0, 0, 1,
    };

    uint8_t const cnfDesc [] = {
        9, 2, 67, 0, 2, 1, 0, 192, 50, // USB Configuration
        9, 4, 0, 0, 1, 2, 2, 0, 0,     // Interface
        5, 36, 0, 16, 1,               // Header Functional
        5, 36, 1, 0, 1,                // Call Management Functional
        4, 36, 2, 2,                   // ACM Functional
        5, 36, 6, 0, 1,                // Union Functional
        7, 5, 130, 3, 8, 0, 255,       // Endpoint 2
        9, 4, 1, 0, 2, 10, 0, 0, 0,    // Data class interface
        7, 5, 1, 2, 64, 0, 0,          // Endpoint 1 out
        7, 5, 129, 2, 64, 0, 0,        // Endpoint 1 in
    };

    void init () {
        RCC(EN_OTGFS, 1) = 1;

        Pin::config("A12:O");
        msIdle(2);
        Pin::config("A11:10,A12");

        USB[GCCFG] = USB[GCCFG] | (1<<21) | (1<<16); // NOVBUSSENS, PWRDWN
        USB[GUSBCFG](30) = 1; // FDMOD
        USB[DCFG](0, 2) = 3;  // DSPD
    }

    auto fifo (int ep) { return USB[(ep+1) * 0x1000]; }

    void sendEp0 (void const* ptr, uint32_t len) {
        if (len > setupPkt.len)
            len = setupPkt.len;

        USB[DIEPTSIZ0] = len;
        USB[DIEPCTL0] = USB[DIEPCTL0] | (1<<31) | (1<<26); // EPENA, CNAK

        uint32_t const* wptr = (uint32_t const*) ptr;
        for (uint32_t i = 0; i < len; i += 4)
            fifo(0) = *wptr++;
    }

    void setConfig () {
        USB[DOEPTSIZ0+0x20] = 64; // accept 64b on RX ep1
        USB[DOEPCTL0+0x20] = (3<<18) | (1<<15) | 64  // BULK ep1
                              | (1<<31) | (1<<26);   // EPENA, CNAK

        USB[DOEPTSIZ0+0x40] = 64; // accept 64b on RX ep2
        USB[DOEPCTL0+0x40] = (2<<18) | (1<<15) | 64  // INTR ep2
                              | (1<<31) | (1<<26);   // EPENA, CNAK
    }

    void poll () {
        uint32_t irq = USB[GINTSTS];
        USB[GINTSTS] = irq; // clear all interrupts

        if (irq & (1<<13)) { // ENUMDNE
            USB[GRXFSIZ] = 512/4;                    // 512b for RX all
            USB[DIEPTXF0] = (128/4<<16) | 512;       // 128b for TX ep0
            USB[DIEPTXF1] = (512/4<<16) | (512+128); // 512b for TX ep1

            USB[DIEPCTL0+0x20] = (1<<22) | (2<<18) | (1<<15) | 64 // fifo1
                                  | (1<<31) | (1<<26); // EPENA, CNAK
            USB[DIEPCTL0+0x40] = (2<<22) | (3<<18) | (1<<15) | 64; // fifo2

            USB[DOEPTSIZ0] = (3<<29) | 64;               // STUPCNT, XFRSIZ
            USB[DOEPCTL0] = (1<<31) | (1<<15) | (1<<26); // EPENA, CNAK
        }

        if ((irq & (1<<4)) && inPending == 0) {
            int rx = USB[GRXSTSP], typ = (rx>>17) & 0xF,
                ep = rx & 0x0F, cnt = (rx>>4) & 0x7FF;

            switch (typ) {
                case 0b0010: // OUT
                    if (ep == 1)
                        inPending = cnt;
                    else {
                        for (int i = 0; i < cnt; i += 4)
                            (uint32_t) fifo(0);
                        if (ep == 0) {
                            setupPkt.len -= cnt;
                            if (setupPkt.len == 0)
                                sendEp0(0, 0);
                        }
                    }
                    break;
                case 0b0011: // OUT complete
                    USB[DOEPCTL0+0x20*ep](26) = 1; // CNAK
                    break;
                case 0b0110: // SETUP
                    ((uint32_t*) &setupPkt)[0] = fifo(0);
                    ((uint32_t*) &setupPkt)[1] = fifo(0);
                    break;
                case 0b0100: { // SETUP complete
                    USB[DOEPCTL0](26) = 1; // CNAK
                    void const* replyPtr = 0;
                    uint32_t replyLen = 0;
                    switch (setupPkt.req) {
                        case 5: // set address
                            USB[DCFG](4, 7) = setupPkt.val;
                            break;
                        case 6: // get descriptor
                            switch (setupPkt.val) {
                                case 0x100: // device desc
                                    replyPtr = devDesc;
                                    replyLen = sizeof devDesc;
                                    break;
                                case 0x200: // configuration desc
                                    replyPtr = cnfDesc;
                                    replyLen = sizeof cnfDesc;
                                    break;
                            }
                            break;
                        case 9: // set configuration
                            setConfig();
                            break;
                        case 34: // set control line state
                            dtr = setupPkt.val & 1;
                            break;
                    }
                    if (setupPkt.len == 0 || (setupPkt.typ & 0x80))
                        sendEp0(replyPtr, replyLen);
                    break;
                }
            }
        }
    }

    bool writable () { poll(); return USB[DTXFSTS0+0x20] > 0; }

    void putc (int c) {
        if (dtr) {
            while (!writable()) {}
            USB[DIEPTSIZ0+0x20] = 1;
            fifo(1) = c;
        }
    }

    bool readable () { poll(); return inReady > 0 || inPending > 0; }

    int getc () {
        while (!readable()) {}

        // get 4 chars from the USB fifo, if needed
        if (inReady == 0) {
            inReady = inPending;
            if (inReady > 4)
                inReady = 4;
            inData = fifo(1);
            inPending -= inReady;
        }

        // consume each of those 4 chars first
        --inReady;
        uint8_t c = inData;
        inData >>= 8;
        return c;
    }
}

int main () {
    fastClock();
    printf("%s: usb @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    led.mode("P");

    usb::init();

    while (true) {
        led = usb::dtr;
        while (usb::readable())
            usb::putc(usb::getc() ^ 0x20); // change case
    }
}
