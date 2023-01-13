struct Peer {
    IpAddr _rIp;
    uint16_t _rPort, _lPort;

    auto operator== (Peer const& p) const {
        return _rPort == p._rPort && _lPort == p._lPort && _rIp == p._rIp;
    }
};

struct TcpListener : Listener {
    enum : uint8_t { FIN=1<<0,SYN=1<<1,RST=1<<2,PSH=1<<3,ACK=1<<4,URG=1<<5 };
    enum : uint8_t { FREE,LSTN,SYNR,SYNS,ESTB,FIN1,FIN2,CSNG,TIMW,CLOW,LACK };

    struct Packet {
        Frame frame;
        Ip4 ip4;
        Tcp tcp;
        uint8_t data [40]; // enough for sending
    } tx;

    uint32_t snd_una, snd_nct, snd_win, rcv_nxt, rcv_wnd;

    TcpListener (Interface& ni, uint16_t port) {
        ni.attach(*this, port);

        mTag = LSTN;
    }

    int onRecv (Message& m) override {
        auto& ni = *(Interface*) mPtr;
        auto& rx = *(Packet*) m.mPtr;

        switch (mTag) {
            case LSTN:
                if (m.mDst == ni.dev && rx.tcp.code == SYN) {
                    gotSyn(m);
                    mTag = SYNR;
                }
                break;
            case SYNR:
                if (m.mDst == ni.dev && (rx.tcp.code & ACK)) {
                    //gotAck(m);
                    mTag = ESTB;
                }
                break;
            case ESTB:
                gotData(m, m.mLen-54-4);
                break;
            default:
                printf("state %d got %c from %d, %d b\n",
                        mTag, m.mTag, m.mDst, m.mLen);
                dumpHex(m.mPtr, m.mLen);
        }

        return 0;
    }

    void gotSyn (Message& m) {
        auto& ni = *(Interface*) mPtr;
        auto& rx = *(Packet*) m.mPtr;

        makeReply(rx);
        tx.tcp.code = SYN | ACK;
        tx.tcp.ack = rx.tcp.seq + 1;
        tx.tcp.win = 536;

        // nasty hack, 6 option bytes: 02 04 02 18 00 00
        OptionIter it {tx.data};
        Net32 mss = 536 << 16;
        it.append(2, &mss, 4); // MSS

        tx.tcp.off = 6 << 4;
        ni.sendUpto(&tx, it.ptr - 2);
    }

    void gotData (Message& m, int len) {
        dumpHex(m.mPtr, len, "TELNET");

        auto& ni = *(Interface*) mPtr;
        auto& rx = *(Packet*) m.mPtr;

        makeReply(rx);
        tx.tcp.code = ACK;
        tx.tcp.ack = rx.tcp.seq + len;
        tx.tcp.win = 536;

        tx.tcp.off = 5 << 4;
        ni.sendUpto(&tx, tx.data);
    }

    void makeReply (Packet const& rx) {
        auto& ni = *(Interface*) mPtr;

        memcpy(&tx, &rx, sizeof tx);

        tx.frame.dst = rx.frame.src;
        tx.frame.src = ni.mac;

        tx.ip4.dstIp = rx.ip4.srcIp;
        tx.ip4.srcIp = ni.ip;

        tx.tcp.sPort = rx.tcp.dPort;
        tx.tcp.dPort = rx.tcp.sPort;
    }
};
