// ethernet driver test app

#include <jee.h>
using namespace jeeh;
#include "board.h"

#include <cstdio> // TODO yuck (snprintf in eth-net.h)
namespace net {
#include <jee/eth-net.h>
#include <jee/eth-tcp.h>
SmallBuf smallBuf;
}
using namespace net;

void uidToMacAddr (MacAddr& mac) {
    auto UID = (uint32_t const*) Signature::uid;
    auto sernum = UID[0] ^ UID[1] ^ UID[2];

    mac.b[0] = 0x2; // locally administered
    mac.b[1] = 0;
    memcpy(mac.b + 2, &sernum, 4);
}

struct NetHandler : Interface {
    ArpListener arp {*this};
    IcmpListener icmp {*this};

    using Interface::Interface; // re-use same constructor

    Message& launch (uint32_t* stack, uint16_t words) {
        auto runner = +[](Message&, void* self) {
            ((NetHandler*) self)->run();
        };
        return os::fork(stack, words, runner, this);
    }

    void run () {
        Message rx {dev, 'R'};
        rx.mTag = 'R';
        os::put(rx); // start receiving packets

        DhcpListener dhcp {*this};

        Pin led (LED_PIN);
        led.mode("P");

        for (auto i = 0; i < 10'000; ++i) {
            auto& m = os::get();
            led = 1;
            if (m.mDst == dev)
                switch (m.mTag) {
                    case 'R':
                        verify(&m == &rx);
                        received(m);
                        os::put(m);
                        break;
                }
            else if (m.mDst == -1) {
                auto p = (Listener*) m.mPtr;
                verify(p != nullptr);
                p->onTimeout(m);
            }
            led = 0;
        }
    }

    void received (Message& m) {
        NetBuf nb = m.mPtr;

        switch (nb.asFrame().typ) {
            case nb.ARP:
                arp.onRecv(m);
                return;

            case nb.IP4:
                switch (nb.asIp4().proto) {
                    case nb.ICMP:
                        icmp.onRecv(m);
                        return;

                    case nb.UDP:
                    case nb.TCP:
                        if (uint16_t port = nb.asUdp().dPort; port != 0) {
                            auto p = findListener(port);
                            if (p != nullptr)
                                printf("onRecv %d\n", p->onRecv(m));
                            else
                                printf("no listener for port %d (from %s)\n",
                                        port, nb.asIp4().srcIp.asStr());
                            return;
                        }
                        break;
                }
                break;
        }

        nb.dump(); // unknown packet type
    }
};

int main () {
    //fastClock();

    Pin backlight ("K3");
    backlight.mode("P"); // turn backlight off

    printf("\n### %s: ether @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    uint32_t stack [500];
    Tasker tasker (stack);

    MacAddr myMac;
    uidToMacAddr(myMac);

    Ethernet eth;
    printf("mac %s dev %d\n", myMac.asStr(), ~eth.devId);

    uint8_t buffer [eth.NRX * eth.BUFSZ];
    eth.init(myMac.b, buffer);

    NetHandler nh (~eth.devId, myMac);
    printf("eth %d b, nh %d b\n", sizeof eth, sizeof nh);

    uint32_t netStack [500];
    nh.launch(netStack, 500);

    TcpListener telnet {nh, 23};

    printf("idling @ %d\n", systick::millis());
    while (tasker.count > 0)
        os::get();

    printf("done\n");
}
