//template <typename T>
//void swap (T& a, T& b) { T t = a; a = b; b = t; }

using SmallBuf = char [20];
extern SmallBuf smallBuf;

struct MacAddr {
    uint8_t b [6];

    auto operator== (MacAddr const& v) const {
        return memcmp(this, &v, sizeof *this) == 0;
    }

    auto asStr (SmallBuf& buf =smallBuf) const {
        auto ptr = buf;
        for (int i = 0; i < 6; ++i)
            ptr += snprintf(ptr, 4, "%c%02x", i == 0 ? ' ' : ':', b[i]);
        return buf + 1;
    }
};

struct Net16 {
    constexpr Net16 (uint16_t v =0) : h ((v<<8) | (v>>8)) {}
    operator uint16_t () const { return (h<<8) | (h>>8); }
private:
    uint16_t h;
};

struct Net32 {
    constexpr Net32 (uint32_t v =0) : b2 {(uint16_t) (v>>16), (uint16_t) v} {}
    operator uint32_t () const { return (b2[0]<<16) | b2[1]; }
private:
    Net16 b2 [2];
};

struct IpAddr : Net32 {
    constexpr IpAddr (uint32_t v =0) : Net32 (v) {}
    constexpr IpAddr (uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4)
        : Net32 ((v1<<24)|(v2<<16)|(v3<<8)|v4) {}

    auto asStr (SmallBuf& buf =smallBuf) const {
        auto p = (uint8_t const*) this;
        auto ptr = buf;
        for (int i = 0; i < 4; ++i)
            ptr += snprintf(ptr, 5, "%c%d", i == 0 ? ' ' : '.', p[i]);
        return buf + 1;
    }
};

constexpr MacAddr wildMac {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
constexpr IpAddr wildIp {255,255,255,255};

struct Frame {
    MacAddr dst, src;
    Net16   typ;
};

struct Arp {
    Net16   hw, proto;
    uint8_t macLen, ipLen;
    Net16   op;
    MacAddr sendMac;
    IpAddr  sendIp;
    MacAddr targMac;
    IpAddr  targIp;
};

struct Ip4 {
    uint8_t versLen, tos;
    Net16   total, id, frag;
    uint8_t ttl, proto;
    Net16   hcheck;
    IpAddr  srcIp, dstIp;
};

struct Icmp {
    uint8_t type, code;
    Net16   sum;
};

struct Udp {
    Net16   sPort, dPort, len, sum;
};

struct Dhcp {
    uint8_t op, htype, hlen, hops;
    Net32   tid;
    Net16   sec, flags;
    IpAddr  clientIp, yourIp, serverIp, gwIp;
    uint8_t clientHw [16], hostName [64], fileName [128];
};

struct Tcp {
    Net16   sPort, dPort;
    Net32   seq, ack;
    uint8_t off, code;
    Net16   win, sum, urg;
};

// most packet types have their payload right after the header
template< typename T >
static uint8_t* payload(T& t) { return (uint8_t*) (&t + 1); }

static uint8_t* payload(Ip4& t) {
    return (uint8_t*) &t + 4 * (t.versLen & 0xF);
}

static uint8_t* payload(Tcp& t) {
    return (uint8_t*) &t + 4 * (t.off >> 4);
}

struct NetBuf {
    enum { ARP = 0x0806, IP4 = 0x0800, ICMP = 1, TCP = 6, UDP = 17 };

    uint8_t* data;

    NetBuf (void* p) : data ((uint8_t*) p) {}

    auto& asFrame () const { return *(Frame*)   data; }
    auto& asArp   () const { return *(Arp*)     payload(asFrame()); }
    auto& asIp4   () const { return *(Ip4*)     payload(asFrame()); }
    auto& asIcmp  () const { return *(Icmp*)    payload(asIp4()); }
    auto& asUdp   () const { return *(Udp*)     payload(asIp4()); }
    auto& asDhcp  () const { return *(Dhcp*)    payload(asUdp()); }
    auto& asTcp   () const { return *(Tcp*)     payload(asIp4()); }
    auto& asData  () const { return *(uint8_t*) payload(asTcp()); }

    void initFrame (uint16_t t) {
        auto& p = asFrame();
        p.typ = t;
    }

    void initIp4 (uint16_t pr) {
        initFrame(IP4);

        auto& p = asIp4();
        p.versLen = 0x45;
        p.ttl = 64;
        p.proto = pr;
        static uint16_t gid;
        p.id = ++gid;
    }

    void initUdp (uint16_t d, uint16_t s) {
        initIp4(17);

        auto& p = asUdp();
        p.sPort = s;
        p.dPort = d;
    }

    void initDhcp () {
        initUdp(67, 68);

        asFrame().dst = wildMac;
        asIp4().dstIp = wildIp;

        auto& p = asDhcp();
        p.op = 1;
        p.htype = 1;
        p.hlen = 6;
        p.flags = 1<<15;
    }

    void setSize (uint32_t len) {
        if (asFrame().typ == IP4) {
            auto& p = asIp4();
            p.total = len - sizeof (Frame);
            if (p.proto == UDP)
                asUdp().len = len - sizeof (Ip4) - sizeof (Frame);
        }
    }

    template< typename T >
    static void dump (T const& t, char const* msg =nullptr) {
        dumpHex(&t, sizeof t, msg);
    }

    void dump () const {
        switch (asFrame().typ) {
            case ARP:
                dump(asArp(), "ARP");
                break;
            case IP4:
                dump(asIp4(), "IP4");
                switch (asIp4().proto) {
                    case ICMP:
                        dump(asIcmp(), "ICMP");
                        break;
                    case UDP:
                        dump(asUdp(), "UDP");
                        if (asUdp().dPort == 68)
                            dump(asDhcp(), "DHCP");
                        break;
                    case TCP:
                        dump(asTcp(), "TCP");
                        break;
                }
                break;
        }
    }

};

struct OptionIter {
    OptionIter (uint8_t* p, uint8_t o =0) : ptr (p), off (o) {}

    operator bool () const { return *ptr != 0xFF; }

    auto next () {
        auto typ = *ptr++;
        len = *ptr++;
        ptr += len;
        return typ;
    }

    void extract (void* p, uint32_t n =0) {
        verify(n == 0 || n == len);
        memcpy(p, ptr-len, len);
    }

    void append (uint8_t typ, void const* p, uint8_t n) {
        *ptr++ = typ;
        *ptr++ = n + off;
        memcpy(ptr, p, n);
        ptr += n;
        *ptr = off ? 0x00 : 0xFF;
    }

    uint8_t* ptr;
    uint8_t len =0;
    uint8_t off;
};

template <int N>
struct ArpCache {
    void init (IpAddr myIp, MacAddr const& myMac) {
        memset(this, 0, sizeof *this);
        prefix = myIp & ~0xFF;
        add(myIp, myMac);
    }

    auto canStore (IpAddr ip) const { return (ip & ~0xFF) == prefix; }

    void add (IpAddr ip, MacAddr const& mac) {
        if (canStore(ip)) {
            // may run out of space, start afresh
            if (items[N-1].node != 0)
                memset(items+1, 0, (N-1) * sizeof items[0]);
            uint8_t b = ip;
            for (auto& e : items)
                if (e.node == 0 || b == e.node || mac == e.mac) {
                    e.node = b;
                    e.mac = mac;
                    return;
                }
        }
    }

    auto toIp (MacAddr const& mac) const -> IpAddr {
        for (auto& e : items)
            if (e.node != 0 && mac == e.mac)
                return prefix + e.node;
        return 0;
    }

    auto toMac (IpAddr ip) const -> MacAddr const& {
        if (canStore(ip))
            for (auto& e : items)
                if ((uint8_t) ip == e.node)
                    return e.mac;
        return wildMac;
    }

    void dump () const {
        printf("ARP cache [%d]\n", N);
        for (auto& e : items)
            if (e.node != 0) {
                SmallBuf sb;
                IpAddr ip = prefix + e.node;
                printf("  %s = %s\n", ip.asStr(), e.mac.asStr(sb));
            }
    }

private:
    struct {
        MacAddr mac;
        uint8_t node; // low byte of IP address, i.e. assumes class C network
    } items [N];
    
    uint32_t prefix =0;
};

struct Listener : Message {
    Listener () : Message {} {}
    virtual ~Listener () =default;

    virtual int onRecv (Message& msg) =0;
    virtual void onTimeout (Message&) { verify(false); }
};

struct Interface {
    int8_t const dev;
    MacAddr const mac;
    IpAddr ip, gw, dns, sub;
    ArpCache<10> arpCache;
    Chain listeners;

    Interface (int d, MacAddr const& m) : dev (d), mac (m) {}

    void attach (Listener& lnr, uint16_t port) {
        lnr.mDst = dev;
        lnr.mLen = port;
        lnr.mPtr = this;
        listeners.append(lnr);
    }

    bool detach (Listener& lnr) {
        return listeners.remove(lnr);
    }

    Listener* findListener (uint16_t port) const {
        verify(port != 0);
        for (auto p = listeners.first(); p != nullptr; p = p->mLnk)
            if (p->mLen == port)
            return (Listener*) p;
        return nullptr;
    }

    void send (void* buffer, uint16_t bytes) {
        printf("send %p #%d @ %d\n", buffer, bytes, systick::millis());
        auto t = systick::micros();

        NetBuf nb = buffer;
        nb.setSize(bytes);

        Message m {dev, 'W', bytes, buffer};
        os::call(m);

        printf("  %d µs\n", systick::micros() - t);
    }

    void sendUpto (void* buffer, void* limit) {
        send(buffer, (uintptr_t) limit - (uintptr_t) buffer);
    }
};

struct ArpListener : Listener {
    struct Packet {
        Frame frame;
        Arp arp;
    } tx;

    ArpListener (Interface& ni) { mPtr = &ni; }

    int onRecv (Message& m) override {
        auto& rx = *(Packet*) m.mPtr;
        auto& ni = *(Interface*) mPtr;

        if (rx.arp.op == 1 && rx.arp.targIp == ni.ip) {
            printf("ARP %s\n", rx.arp.sendIp.asStr());

            //tx = rx; // TODO why is this not identical?
            memcpy(&tx, &rx, sizeof tx);

            tx.frame.dst = rx.frame.src;
            tx.frame.src = ni.mac;

            tx.arp.op = 2; // ARP reply
            tx.arp.targMac = rx.arp.sendMac;
            tx.arp.targIp = rx.arp.sendIp;
            tx.arp.sendMac = ni.mac;
            tx.arp.sendIp = ni.ip;

            ni.send(&tx, sizeof tx);
        }

        return 0;
    }
};

struct IcmpListener : Listener {
    struct Packet {
        Frame frame;
        Ip4 ip4;
        Icmp icmp;
        struct {
            Net16 id;
            Net16 seq;
            uint8_t time [8];
            uint8_t data [48];
        } echo;
    } tx;

    IcmpListener (Interface& ni) { mPtr = &ni; }

    int onRecv (Message& m) override {
        auto& rx = *(Packet*) m.mPtr;
        auto& ni = *(Interface*) mPtr;

        printf("ICMP %s type %d\n", rx.ip4.srcIp.asStr(), rx.icmp.type);
        if (rx.icmp.type == 8) {

            //tx = rx; // TODO why is this not identical?
            memcpy(&tx, &rx, sizeof tx);

            tx.frame.dst = rx.frame.src;
            tx.frame.src = ni.mac;

            tx.ip4.dstIp = rx.ip4.srcIp;
            tx.ip4.srcIp = ni.ip;
            tx.icmp.type = 0; // echo reply
            tx.icmp.sum = 0; // checksum

            ni.send(&tx, sizeof tx);
        }

        return 0;
    }
};

struct DhcpListener : Listener {
    enum { Subnet=1,Router=3,Dns=6,HostName=12,Domain=15,Bcast=28,Ntps=42,
            ReqIp=50,Lease=51,MsgType=53,ServerIp=54,ReqList=55 };

    enum { INIT, SELECT, REQUEST, BOUND }; // TODO renew/rebind

    Message timer {-1, 0, 25, this};

    struct Packet {
        Frame frame;
        Ip4 ip4;
        Udp udp;
        Dhcp dhcp;
        uint8_t data [40]; // enough for sending
    } tx;

    DhcpListener (Interface& ni) {
        ni.attach(*this, 68);

        mTag = INIT;
        os::put(timer);
    }

    int onRecv (Message& m) override {
        auto& rx = *(Packet*) m.mPtr;
        switch (mTag) {
            case SELECT:
                if (rx.data[4] == MsgType && rx.data[6] == 2)
                    mTag = gotOffer (rx);
                break;
            case REQUEST:
                if (rx.data[4] == MsgType && rx.data[6] == 5)
                    mTag = gotAck (rx);
                break;
            default:
                return -1;
        };
        return 0;
    }

    void onTimeout (Message&) override {
        auto& ni = *(Interface*) mPtr;

        Message msg {ni.dev, 'L'};
        os::call(msg); // never blocks
        if (msg.mLen == 0) {
            mTag = INIT;
            os::put(timer);
            return; // link is down, poll again
        }

        mTag = SELECT; // start acquiring a DHCP lease

        memset(&tx, 0, sizeof tx);
        NetBuf nb = &tx;
        nb.initDhcp();

        tx.frame.src = ni.mac;
        (MacAddr&) tx.dhcp.clientHw = ni.mac;

        uint8_t const cookie [] {99, 130, 83, 99};
        memcpy(tx.data, cookie, sizeof cookie);

        OptionIter it {tx.data+4};
        it.append(MsgType, "\x01", 1);         // discover
        it.append(ReqList, "\x01\x03\x06", 3); // subnet router dns

        ni.sendUpto(&tx, it.ptr + 1);
    }

    int gotOffer (Packet& rx) {
        auto& ipOffer = rx.dhcp.yourIp;
        auto& ni = *(Interface*) mPtr;

        OptionIter it {tx.data+4};
        it.append(MsgType, "\x03", 1); // request
        it.append(ReqIp, &ipOffer, 4);
        it.append(HostName, "f750", 4); // host name TODO hardcoded ...

        ni.sendUpto(&tx, it.ptr + 1);
        return REQUEST;
    }

    int gotAck (Packet& rx) {
        auto& ni = *(Interface*) mPtr;
        ni.ip = rx.dhcp.yourIp;

        OptionIter it {rx.data+4};
        while (it) {
            auto typ = it.next();
            switch (typ) {
                case Subnet: it.extract(&ni.sub, 4); break;
                case Dns:    it.extract(&ni.dns, 4); break;
                case Router: it.extract(&ni.gw,  4); break;
            }
        }

        SmallBuf sb [3];
        printf("DHCP %s gw %s sub %s dns %s\n",
                ni.ip.asStr(), ni.gw.asStr(sb[0]),
                ni.sub.asStr(sb[1]), ni.dns.asStr(sb[2]));

        ni.arpCache.init(ni.ip, ni.mac);
        ni.arpCache.add(rx.ip4.srcIp, rx.frame.src); // assumes dhcp == gateway
        ni.arpCache.dump();

        return BOUND;
    }
};

static_assert(sizeof (Frame)  == 14);
static_assert(sizeof (Arp)    == 28);
static_assert(sizeof (Ip4)    == 20);
static_assert(sizeof (Icmp)   == 4);
static_assert(sizeof (Udp)    == 8);
static_assert(sizeof (Dhcp)   == 236);
static_assert(sizeof (Tcp)    == 20);
