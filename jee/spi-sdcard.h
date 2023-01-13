struct SdCard {
    constexpr static auto TIMEOUT = 50000; // arbitrary

    jeeh::SpiBase& spi;

    SdCard (jeeh::SpiBase& s) : spi (s) {}

    void init () {
        for (int i = 0; i < 10; ++i)
            spi.xfer(0xFF);

        auto r = cmd(0, 0, 0x95);
        //printf("c0 %d\n", r);

        r = cmd(8, 0x1AA, 0x87);
        //printf("c8 %d %08x\n", r, get32());

        do {
            cmd(55, 0);
            r = cmd(41, 1<<30);
        } while (r == 1);
        //printf("c41-1 %d\n", r);

        do {
            cmd(55, 0);
            r = cmd(41, 0);
        } while (r == 1);
        //printf("c41-0 %d\n", r);

        do {
            r = cmd(58, 0);
        } while (r == 1);
        auto v = get32();
        //printf("c58 %d %08x\n", r, v);
        sdhc = (v & (1<<30)) != 0;

        do {
            r = cmd(16, 512);
        } while (r == 1);
        //printf("c16 %d\n", r);

        spi.disable();
    }

    auto readBlock (uint32_t page, uint8_t* buf) const -> int {
        int last = cmd(17, sdhc ? page : page * 512);
        for (int i = 0; last != 0xFE; ++i) {
            if (++i >= TIMEOUT)
                return 0;
            last = spi.xfer(0xFF);
        }
        for (int i = 0; i < 512; ++i)
            *buf++ = spi.xfer(0xFF);
        spi.xfer(0xFF);
        spi.disable();
        return 512;
    }

    auto writeBlock (uint32_t page, uint8_t const* buf) const -> int {
        cmd(24, sdhc ? page : page * 512);
        spi.xfer(0xFF);
        spi.xfer(0xFE);
        for (int i = 0; i < 512; ++i)
            spi.xfer(*buf++);
        spi.xfer(0xFF);
        spi.disable();
        return 512;
    }

    bool sdhc =false;
private:
    auto cmd (int req, uint32_t arg, uint8_t crc =0) const -> int {
        spi.disable();
        spi.enable();
        wait();

        spi.xfer(0x40 | req);
        spi.xfer(arg >> 24);
        spi.xfer(arg >> 16);
        spi.xfer(arg >> 8);
        spi.xfer(arg);
        spi.xfer(crc);

        for (int i = 0; i < 1000; ++i)
            if (uint8_t r = spi.xfer(0xFF); r != 0xFF)
                return r;

        return -1;
    }

    void wait () const {
        for (int i = 0; i < TIMEOUT; ++i)
            if (spi.xfer(0xFF) == 0xFF)
                return;
    }

    auto get32 () const -> uint32_t {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v = (v<<8) | spi.xfer(0xFF);
        return v;
    }
};
