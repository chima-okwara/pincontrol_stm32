// Quad SPI for the 16 MBit flash on Disco-F750

namespace qspi {
using namespace jeeh;

// see ST's ref man: RM0385 (QUADSPI)
enum { CR=0x00, DCR=0x04, SR=0x08, DLR=0x10, CCR=0x14, AR=0x18, DR=0x20,
        PSMKR=0x24, PSMAR=0x28, PIR=0x2C, LPTR=0x30 };

const auto addr = (uint32_t const*) 0x9000'0000;
constexpr auto fsize = 24; // flash has 16 MB, 2^24 bytes
constexpr auto dummy = 10;  // number of cycles between cmd and data

void waitBusy () {
    while (QUADSPI[SR](5)) {} // wait until not busy
}

void pollDone () {
    waitBusy();
    QUADSPI[DLR] = 0;
    QUADSPI[PIR] = 0x10;
    QUADSPI[PSMKR] = 1<<0;
    QUADSPI[PSMAR] = 0;
    // mem-mapped: FMODE DMODE, IMODE INS
    QUADSPI[CCR] = (2<<26) | (3<<24) | (3<<8) | (0x05<<0); // poll status
    waitBusy();
}

void mmapEnable () {
    waitBusy();
    // mem-mapped: FMODE DMODE DCYC, ADSIZE ADMODE IMODE INS
    QUADSPI[CCR] = (3<<26) | (3<<24) | (dummy<<18) |
                    (2<<12) | (3<<10) | (3<<8) | (0xEB<<0);
}

void init () {
    RCC(EN_QSPI, 1) = 1;

    Pin::config("B6:PV10,B2:PV9,D11,D12,D13,E2");

    QUADSPI[CR] = (1<<24) | (1<<22) | (1<<3) | (1<<0); // PRESCALER APMS TCEN EN
    QUADSPI[DCR] = ((fsize-1)<<16); // FSIZE
    QUADSPI[LPTR] = (1<<10); // raise nsel after 1k idle cycles

    waitBusy();
    // indirect: IMODE INS
    QUADSPI[CCR] = (1<<8) | (0x35<<0); // enter QPI mode for INS+ADDR+DATA

    mmapEnable();
}

void deinit () {
    RCC(EN_QSPI, 1) = 0;
}

void wipe () {
    waitBusy();
    // indirect: IMODE INS
    QUADSPI[CCR] = (3<<8) | (0x06<<0); // write enable

    waitBusy();
    QUADSPI[CCR] = (3<<8) | (0xC7<<0); // chip erase
    pollDone();
    mmapEnable();
}

void read (uint32_t addr, uint32_t* ptr, int num) {
    waitBusy();
    QUADSPI[DLR] = 4*num-1;
    QUADSPI[AR] = addr;
    // indirect: FMODE DMODE DCYC, ADSIZE ADMODE IMODE INS
    QUADSPI[CCR] = (1<<26) | (3<<24) | (dummy<<18) |
                    (2<<12) | (3<<10) | (3<<8) | (0xEB<<0); // read bytes
    for (int i = 0; i < num; ++i)
        ptr[i] = QUADSPI[DR];
    pollDone();

    mmapEnable();
}

void write (uint32_t addr, uint32_t const* ptr, int num) {
    waitBusy();
    // indirect: IMODE INS
    QUADSPI[CCR] = (3<<8) | (0x06<<0); // write enable
    waitBusy();
    QUADSPI[DLR] = 4*num-1;
    QUADSPI[AR] = addr;
    // indirect: DMODE ADSIZE ADMODE IMODE INS // program bytes
    QUADSPI[CCR] = (3<<24) | (2<<12) | (3<<10) | (3<<8) | (0x3E<<0);

    waitBusy();
    for (int i = 0; i < num; ++i)
        QUADSPI[DR] = ptr[i];
    pollDone();

    mmapEnable();
}

} // namespace qspi
