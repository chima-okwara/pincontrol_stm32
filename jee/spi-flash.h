namespace jeeh {

struct SpiFlash {
    SpiBase& spi;

    SpiFlash (SpiSoft& s) : spi (s) {}

    void reset () const {
        cmd(0x66);
        spi.disable();
        cmd(0x99);
        spi.disable();
    }

    int devId () const {
        cmd(0x9F);
        int r = spi.xfer(0) << 16;
        r |= spi.xfer(0) << 8;
        r |= spi.xfer(0);
        spi.disable();
        return r;
    }

    int size () const {
        // works for WinBond W25Qxx, e.g. W25Q64 => 0xC84017 => 8192 KB
        return 1 << ((devId() & 0xFF) - 10);
    }

    void wipe () const {
        wcmd(0xC7); // 0x60 doesn't work on Micron Tech (N25Q)
        wait();
    }

    void erase (int page) const {
        wcmd(0x20);
        w24b(page<<8);
        wait();
    }

    void read256 (int page, uint8_t* buf) const {
        read(page<<8, buf, 256);
    }

    void read (int offset, uint8_t* buf, int cnt) const {
        cmd(0x0B);
        w24b(offset);
        spi.xfer(0);
        spi.block(nullptr, buf, cnt);
        spi.disable();
    }

    void write256 (int page, const uint8_t* buf) const {
        write(page<<8, buf, 256);
    }

    void write (int offset, const uint8_t* buf, int cnt) const {
        wcmd(0x02);
        w24b(offset);
        spi.block(buf, nullptr, cnt);
        wait();
    }

//private:
    void cmd (int arg) const {
        spi.enable();
        spi.xfer(arg);
    }
    void wait () const {
        spi.disable();
        cmd(0x05);
        while (spi.xfer(0) & 1) {}
        spi.disable();
    }
    void wcmd (int arg) const {
        wait();
        cmd(0x06);
        spi.disable();
        cmd(arg);
    }
    void w24b (int offset) const {
        spi.xfer(offset >> 16);
        spi.xfer(offset >> 8);
        spi.xfer(offset);
    }
};

} // namespace jeeh
