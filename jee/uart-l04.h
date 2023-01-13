struct Uart : Device {
    struct Config {
        uint32_t uart;
        uint16_t ena;
        uint8_t mhz;
        Irq idleIrq, rxIrq, txIrq;
        uint8_t dma :1, rxChan :3, txChan :3, rxReq :4, txReq :4; // 0-based
    };

    auto devReg (int off) const { IoReg<0> io; return io[dev.uart+off]; }
    auto dmaReg (int off) const { return DMA1[0x400*dev.dma+off]; }
    auto dmaRX (int off) const { return dmaReg(off+CHAN_STEP*dev.rxChan); }
    auto dmaTX (int off) const { return dmaReg(off+CHAN_STEP*dev.txChan); }

    void init (Config const& config, char const* desc, uint32_t bd =115200) {
        dev = config;
        Pin::config(desc);
        RCC(EN_DMA1+dev.dma, 1) = 1; // dma on
        RCC(dev.ena, 1) = 1;         // uart on

        dmaReg(CSELR)(4*(dev.rxChan), 4) = dev.rxReq;
        dmaReg(CSELR)(4*(dev.txChan), 4) = dev.txReq;

        dmaRX(CNDTR) = sizeof rxBuf;
        dmaRX(CMAR) = (uint32_t) rxBuf;
        dmaRX(CPAR) = dev.uart + RDR;
        dmaRX(CCR) = 0b1010'0111; // MINC CIRC HTIE TCIE EN

        dmaTX(CNDTR) = 0;
        dmaTX(CPAR) = dev.uart + TDR;
        dmaTX(CCR) = 0b1001'0010; // MINC DIR TCIE

        baud(bd);
        devReg(CR3) = 0b1100'0000; // DMAT DMAR
        devReg(CR1) = 0b0001'1101; // IDLEIE TE RE UE

        irqInstall((int) dev.idleIrq); // uart
        irqInstall((int) dev.rxIrq);   // dma rx
        irqInstall((int) dev.txIrq);   // dma tx
    }

    void deinit () {
        RCC(dev.ena, 1) = 0;          // uart off
        RCC(EN_DMA1+dev.dma, 1) = 0;  // dma off
    }

    void baud (uint32_t bd) const {
        auto n = SystemCoreClock;
        while (n > dev.mhz * 1'000'000)
            n /= 2;
        devReg(BRR) = n / bd;
    }

    void start (Message& m) override {
        switch (m.mTag) {
            case 'R':
                if (m.mLen > 0) {
                    rxFill = (rxFill + m.mLen) % sizeof rxBuf;
                    if (m.mPtr == nullptr)
                        break;
                }
                m.mLen = rxAvail();
                if (m.mLen > 0) {
                    m.mPtr = rxBuf + rxFill;
                    reply(&m);
                } else
                    rxMsgs.append(m);
                break;
            case 'W':
                if (!txMsgs.append(m))
                    txStart();
                break;
            default:
                m.mTag = -1;
                reply(&m);
        }
    }

    void finish () override {
        auto n = rxAvail();
        if (n > 0) {
            auto mp = rxMsgs.pull();
            if (mp != nullptr) {
                mp->mPtr = rxBuf + rxFill;
                mp->mLen = n;
                reply(mp);
            }
        }
        if (dmaTX(CCR)(0) == 0) {
            reply(txMsgs.pull());
            txStart();
        }
    }

    void abort (Message& m) override {
        if (rxMsgs.remove(m))
            return;
        // FIXME not quite right for both head and non-head removal
        if (txMsgs.remove(m)) {
            txStart();
            return;
        }
    }

    Config dev;
private:
    uint8_t rxBuf [10];
    uint16_t rxFill =0; // where the next data comes from
    Chain rxMsgs, txMsgs;

    uint32_t rxAvail () const {
        int n = sizeof rxBuf - rxFill - dmaRX(CNDTR);
        return n >= 0 ?  n : sizeof rxBuf - rxFill;
    }

    void txStart () {
        auto mp = txMsgs.first();
        if (mp != nullptr) {
            dmaTX(CMAR) = (uint32_t) mp->mPtr;
            dmaTX(CNDTR) = mp->mLen;
            dmaTX(CCR)(0) = 1; // EN
        }
    }

    // the actual interrupt handler, with access to the uart object
    bool interrupt (int irq) override {
        // FIXME assumes L4 w/ 3 irq sources, but not quite right for L0
        if (irq == (int) dev.idleIrq) {
            devReg(CR) = 0b0001'1111; // clear idle and error flags

            if (!rxMsgs.isEmpty() && rxAvail() > 0)
                return true; // rx done
        } else if (irq == (int) dev.rxIrq) {
            auto r = 4*dev.rxChan;
            dmaReg(IFCR) = 1 << r;

            if (!rxMsgs.isEmpty() && rxAvail() > 0)
                return true; // rx done
        } else {
            auto t = 4*dev.txChan;
            dmaReg(IFCR) = 1 << t;
            dmaTX(CCR)(0) = 0; // ~EN
            return true; // tx done
        }
        return false;
    }

    enum { CR1=0x00,CR3=0x08,BRR=0x0C,SR=0x1C,CR=0x20,RDR=0x24,TDR=0x28 };
    enum { ISR=0x00,IFCR=0x04,CCR=0x08,CNDTR=0x0C,CPAR=0x10,CMAR=0x14,CSELR=0xA8 };
    enum { CHAN_STEP=0x14 };
};
