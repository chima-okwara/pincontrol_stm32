// measure how long some basic talls take, i.e. pendsv, svc, send, read, etc

#include <jee.h>
using namespace jeeh;

void one (Message&, void*) {
    os::delay(5);
    uint32_t t;

    t = cycles::count();
    t = cycles::count() - t;
    printf("none\t%4d\n", t);

    t = cycles::count();
    asm ("isb");
    t = cycles::count() - t;
    printf("isb\t%4d\n", t);

    t = cycles::count();
    asm ("svc 0");
    t = cycles::count() - t;
    printf("svc\t%4d\n", t);

    t = cycles::count();
    SCB[0x04] = 1<<28; // ICSR = PENDSVSET
    asm ("isb");
    t = cycles::count() - t;
    printf("pendsv\t%4d\n", t);

    Message msg {1}; // self
    t = cycles::count();
    os::call(msg);
    t = cycles::count() - t;
    printf("call #1\t%4d\n", t);

    verify(msg.mDst == 1);
    t = cycles::count();
    os::put(msg);
    t = cycles::count() - t;
    printf("put  #1\t%4d\n", t);

    t = cycles::count();
    auto& p = os::get();
    t = cycles::count() - t;
    printf("recv #1\t%4d\n", t);
    verify(&p == &msg);

    msg.mDst = 2;
    t = cycles::count();
    os::call(msg);
    t = cycles::count() - t;
    printf("call #2\t%4d\n", t);

    os::delay(1);

    verify(msg.mDst == 2);
    t = cycles::count();
    os::put(msg);
    t = cycles::count() - t;
    printf("put  #2\t%4d\n", t);

    t = cycles::count();
    auto& q = os::get();
    t = cycles::count() - t;
    printf("recv #2\t%4d\n", t);
    verify(&q == &msg);
}

void two (Message&, void*) {
    os::put(os::get()); // reply to call #2
    os::delay(1);

    os::put(os::get()); // reply to put #2
    os::delay(10);
}

int main () {
    //fastClock();
    printf("\n### %s: svc @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);
    cycles::init();

    uint32_t stack [250];
    Tasker tasker (stack);

    uint32_t oneStack [150];
    os::fork(oneStack, one);

    uint32_t twoStack [150];
    os::fork(twoStack, two);

    printf("idling\n");
    while (tasker.count > 0)
        os::get();

    printf("done\n");
}
