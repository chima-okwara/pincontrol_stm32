struct Ethernet : Device {
    constexpr static auto DMA = ETHERNET_DMA;
    enum { BMR=0x00,TPDR=0x04,RPDR=0x08,RDLAR=0x0C,TDLAR=0x10,
           DSR=0x14,OMR=0x18,IER=0x1C };

    constexpr static auto MAC = ETHERNET_MAC;
    enum { CR=0x00,FFR=0x04,HTHR=0x08,HTLR=0x0C,MIIAR=0x10,MIIDR=0x14,
           FCR=0x18,MSR=0x38,IMR=0x3C,A0HR=0x40,A0LR=0x44 };

    auto readPhy (int reg) -> uint16_t {
        MAC[MIIAR] = (0<<11)|(reg<<6)|(0b100<<2)|(1<<0);
        while (MAC[MIIAR](0) == 1) {} // wait until MB clear
        return MAC[MIIDR];
    }

    void writePhy (int reg, uint16_t val) {
        MAC[MIIDR] = val;
        MAC[MIIAR] = (0<<11)|(reg<<6)|(0b100<<2)|(1<<1)|(1<<0);
        while (MAC[MIIAR](0) == 1) {} // wait until MB clear
    }

    struct DmaDesc {
        int32_t volatile stat;
        uint32_t size;
        uint8_t* data;
        DmaDesc* next;
        //uint32_t extStat, gap, times [2];

        auto available () const { return stat >= 0; }

        void release () {
            asm volatile ("dsb"); stat |= (1<<31); asm volatile ("dsb");
        }
    };

    void init (uint8_t const* mac, uint8_t* buffer) {
        // F7508-DK pins, all using alt mode 11:
        //      A1: refclk, A2: mdio, A7: crsdiv, C1: mdc, C4: rxd0,
        //      C5: rxd1, G2: rxer, G11: txen, G13: txd0, G14: txd1,
        Pin::config("A1:PV11,A2,A7,C1,C4,C5,G2,G11,G13,G14");

        RCC[AHB1ENR](25, 3) = 0b111; // ETHMAC EN,RXEN,TXEN in AHB1ENR

        RCC(EN_SYSCFG, 1) = 1; // SYSCFGEN in APB2ENR
        SYSCFG[0x04](23) = 1; // RMII_SEL in PMC

        writePhy(0, 0x8000); // PHY reset

        DMA[BMR](0) = 1; // SR in DMABMR
        while (DMA[BMR](0) != 0) {}
        // not set: MAC[FFR] MAC[HTHR] MAC[HTLR] MAC[FCR]
        DMA[BMR] = // AAB USP RDP FB PM PBL EDFE DA
            (1<<25)|(1<<24)|(1<<23)|(32<<17)|(1<<16)|(1<<14)|(32<<8)|(1<<7)|(1<<1);

        MAC[A0HR] = ((uint16_t const*) mac)[2];
        MAC[A0LR] = ((uint32_t const*) mac)[0];

        for (int i = 0; i < NTX; ++i) {
            txDesc[i].stat = 0; // not owned by DMA
            txDesc[i].next = txDesc + (i+1) % NTX;
        }

        for (int i = 0; i < NRX; ++i) {
            rxDesc[i].stat = (1<<31); // OWN
            rxDesc[i].size = (1<<14) | (BUFSZ - 2); // RCH SIZE
            rxDesc[i].data = buffer + i * BUFSZ + 2;
            rxDesc[i].next = rxDesc + (i+1) % NRX;
        }

        DMA[TDLAR] = (uint32_t) txDesc;
        DMA[RDLAR] = (uint32_t) rxDesc;

        DMA[OMR] = (1<<21)|(1<<20)|(1<<13)|(1<<1); // TSF FTF ST SR

        MAC[IMR] = (1<<9)|(1<<3); // TSTIM PMTIM
        DMA[IER]= (1<<16)|(1<<6)|(1<<0); // NISE RIE TIE

        irqInstall((uint8_t) Irq::ETH);

        MAC[CR] = 1<<15; // start inactive, need to wait for link up
    }

    void deinit () {
        RCC(EN_SYSCFG, 1) = 0;
        RCC[AHB1ENR](25, 3) = 0;
    }

    void start (Message& m) override {
        switch (m.mTag) {
            case 'L':
                m.mLen = checkLink();
                reply(&m);
                break;
            case 'R':
                rxMsgs.append(m);
                rxStart();
                break;
            case 'W':
                txMsgs.append(m);
                txStart();
                break;
            default:
                verify(0);
                m.mTag = -1;
                reply(&m);
        }
        finish();
    }

    void finish () override {
        // asm ("dsb"); // TODO when? where? why?
        while (!rxMsgs.isEmpty() && rxDesc[rxNext].available()) {
            auto& desc = rxDesc[rxNext];
            auto mp = rxMsgs.pull();
            mp->mLen = (desc.stat >> 16) & 0xFFF;
            mp->mPtr = desc.data;
            reply(mp);
            rxStart();
        }
        while (!txMsgs.isEmpty() && txDesc[txNext].available()) {
            reply(txMsgs.pull());
            txStart();
        }
    }

    void abort (Message& m) override {
        if (rxMsgs.remove(m))
            rxStart();
        else if (txMsgs.remove(m))
            txStart();
    }

    constexpr static auto NRX = 3, NTX = 3, BUFSZ = 592; // or 1524 for max eth
private:
    DmaDesc rxDesc [NRX], txDesc [NTX];
    uint8_t rxNext = 0, txNext = 0;
    Chain rxMsgs, txMsgs;

    int checkLink () {
        if ((readPhy(1) & (1<<2)) == 0) { // link down
            MAC[CR] = 1<<15;
            return 0;
        }
        if (MAC[CR](2) == 0) { // link went fron down to up
            auto r = readPhy(31);
            auto duplex = (r>>4) & 1, fast = (r>>3) & 1;
            printf("link up, full-duplex %d, 100-Mbit/s %d @ %d\n",
                    duplex, fast, systick::millis());
            // APCS IPCO
            MAC[CR] = (1<<15)|(fast<<14)|(duplex<<11)|(1<<10)|(1<<7)|(3<<2);
        }
        return 1;
    }

    void rxStart () {
        Message* mp = rxMsgs.first();
        if (mp != nullptr && rxDesc[rxNext].available()) {
            rxDesc[rxNext].stat = 0;
            rxDesc[rxNext].release();
            rxNext = (rxNext + 1) % NRX;
            DMA[RPDR] = 0; // resume rx DMA
        }
    }

    void txStart () {
        Message* mp = txMsgs.first();
        if (mp != nullptr && txDesc[txNext].available()) {
            txDesc[txNext].size = mp->mLen;
            txDesc[txNext].data = (uint8_t*) mp->mPtr;
            txDesc[txNext].stat = (0b0111<<28)|(3<<22)|(1<<20); // IC LS FS CIC TCH
            txDesc[txNext].release();
            txNext = (txNext + 1) % NTX;
            DMA[TPDR] = 0; // resume tx DMA
        }
    }

    bool interrupt (int) override {
        uint32_t dsr = DMA[DSR];
        DMA[DSR] = dsr; // clear all
        return (dsr & ((1<<6)|(1<<0))) != 0;
    }
};
