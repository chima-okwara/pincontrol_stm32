#if !defined(DEBUGF) && !defined(NDEBUG)
#define DEBUGF "t"
#endif

#include "jee.h"

namespace jeeh {

uint8_t irqs [(int) Irq::limit];
Device* devices [20];  // must be large enough to hold all device objects
uint8_t devNext;

struct Task : Message, Chain {
    enum { RUN=0x00, WAIT=0x01, DEAD=0x02 };

    uint32_t* sp;
    int8_t tid;
    uint8_t state;

    int init (uint32_t* top);
    void deinit ();

    void submit (Message&);
    Message& wait ();
    void reschedule (int newState);
    void show ();
};
static_assert(sizeof (Task) == 24);

Task* tasks [32];
int8_t currTask, nextTask;
uint32_t pending;

bool Chain::insert (Message& msg) {
    verify(!msg.inUse());
    msg.mLnk = cHead;
    cHead = &msg;
    return msg.mLnk == nullptr;
}

bool Chain::append (Message& msg) {
    verify(!msg.inUse());
    auto pp = &cHead;
    while (*pp != nullptr)
        pp = &(*pp)->mLnk;
    msg.mLnk = nullptr;
    *pp = &msg;
    return pp != &cHead;
}

bool Chain::remove (Message& msg) {
    verify(msg.inUse());
    for (auto pp = &cHead; *pp != nullptr; pp = &(*pp)->mLnk)
        if (*pp == &msg) {
            *pp = msg.mLnk;
            msg.mLnk = &msg;
            return true;
        }
    return false;
}

Message* Chain::pull () {
    auto mp = cHead;
    if (mp != nullptr) {
        cHead = mp->mLnk;
        mp->mLnk = mp;
    }
    return mp;
}

Device::Device () {
    verify(devNext < sizeof devices / sizeof *devices);
    devId = devNext++;
    devices[devId] = this;
}

void Device::irqInstall (int num) const {
    irqs[num] = devId;
    nvicEnable(num);
}

void Device::irqTrigger (int num) {
    if (interrupt(num)) {
#if STM32L0
        { BlockIRQ irq; pending |= 1 << devId; }
#else
        __atomic_or_fetch(&pending, 1 << devId, __ATOMIC_RELAXED);
#endif
        SCB[0x04] = 1<<28; // ICSR PENDSVSET
    }
}

void Device::reply (Message* mp) {
    if (mp != nullptr) {
        auto tp = tasks[mp->mDst];
        verify(tp != nullptr);
        mp->mDst = ~devId; // restore original destination, i.e. this driver
        tp->submit(*mp);
    }
}

void Device::nvicEnable (int num) {
    NVIC[0x00 + 4*(num/32)] = 1 << num%32;
}

void Device::nvicDisable (int num) {
    NVIC[0x80 + 4*(num/32)] = 1 << num%32;
    asm volatile ("dsb; isb");
}

extern "C" {
    void irqDispatch () {
        uint8_t irq = SCB[0x4] - 16; // ICSR
        auto idx = irqs[irq];
        devices[idx]->irqTrigger(irq);
    }

    // to re-generate "jee-irqs.h", see the "all-irqs.sh" script
    #define IRQ(f)      [[gnu::alias ("irqDispatch")]] void f ();
    #include "jee-irqs.h"
    #undef IRQ
}

struct Ticker : Device, Chain {
    Ticker () { verify(devId == 0); }

    void start (Message& msg) override {
        verify(msg.mLen <= 60'000);
        auto t = systick::millis();

        auto pp = &cHead; // insert in proper position
        while (*pp != nullptr && msg.mLen >= (uint16_t) ((*pp)->mLen - t))
            pp = &(*pp)->mLnk;

        msg.mLen += t; // make absolute, truncated to 16 bits
        msg.mLnk = *pp;
        *pp = &msg;
    }

    bool interrupt (int) override {
        return expired();
    }

    void finish () override {
        while (expired())
            reply(pull());
    }

    void abort (Message& msg) override {
        remove(msg);
    }

    bool expired () const {
        return cHead != nullptr &&
                (uint16_t) (cHead->mLen - systick::millis() - 1) > 60'000;
    }
};

Ticker ticker;

#if STM32F1
#define SYSCFG      AFIO
#define EN_SYSCFG   EN_AFIO
#elif STM32L0 && !STM32L073xx
#define SYSCFG      SYSCFG_COMP
#endif

#if STM32F3
#define EXTI2       EXTI2_TSC
#endif

ExtIrq::ExtIrq () {
    RCC(EN_SYSCFG, 1) = 1;
#if STM32L0
    irqInstall((int) Irq::EXTI0_1);
    irqInstall((int) Irq::EXTI2_3);
    irqInstall((int) Irq::EXTI4_15);
#else
    irqInstall((int) Irq::EXTI0);
    irqInstall((int) Irq::EXTI1);
    irqInstall((int) Irq::EXTI2);
    irqInstall((int) Irq::EXTI3);
    irqInstall((int) Irq::EXTI4);
    irqInstall((int) Irq::EXTI9_5);
    irqInstall((int) Irq::EXTI15_10);
#endif
}

#if STM32H7
enum { EXTICR1=0x08, RTSR=0x00, FTSR=0x04, IMR=0x80, PR=0x88 }; // cpu1
#else
enum { EXTICR1=0x08, IMR=0x00, RTSR=0x08, FTSR=0x0C, PR=0x14 };
#endif

void ExtIrq::start (Message& m) {
    auto sel = m.mTag - 'A';
    auto pos = m.mLen;
    auto mode = (int) m.mPtr; // NONE / RISE / FALL / BOTH
    append(m);

    auto off = 4*(pos%4) + 32*(pos/4);
    SYSCFG[EXTICR1](off, 4) = sel;

    if (mode != 0) {
        EXTI[RTSR](pos) = (mode & RISE) != 0;
        EXTI[FTSR](pos) = (mode & FALL) != 0;
        EXTI[IMR](pos) = 1;
    } else
        EXTI[IMR](pos) = 0;
}

void ExtIrq::finish () {
#if STM32L0
    uint32_t f; { BlockIRQ irq; f = flags; flags = 0; }
#else
    auto f = __atomic_exchange_n(&flags, 0, __ATOMIC_RELAXED);
#endif

    auto pp = &cHead;
    while (*pp != nullptr)
        if (f & (1 << (*pp)->mLen)) {
            auto r = *pp;
            *pp = r->mLnk;
            r->mLnk = r;
            reply(r);
        } else
            pp = &(*pp)->mLnk;
}

void ExtIrq::abort (Message& m) {
    remove(m);
}

bool ExtIrq::interrupt (int) {
    flags |= EXTI[PR];
    EXTI[PR] = flags; // clear
    return flags != 0;
}

Tasker::Tasker (uint32_t* ptr, uint32_t len) {
    // stay in protected thread mode, switch to separate stacks
    asm volatile (
        " mov r3,sp      \n"
        " msr psp,r3     \n"
        " mov r3,#2      \n"
        " msr control,r3 \n"
        " isb            \n"
        " msr msp,%0     \n"
    :: "r" ((uintptr_t) (ptr + len)) : "r3");

    // SysTick can be low-priority, it's only used to wake up timed-out tasks
    ((uint8_t*) SCB.ADDR)[0x23] = 0xFF;
    
    // PendSV is used to switch stacks with the lowest interrupt priority
    //  (SHPR3, PRI_14) it always runs last, i.e. as only active exception
    ((uint8_t*) SCB.ADDR)[0x22] = 0xFF;

    // SVC is used to protect kernel-specific non-preemptible actions
    //  (SHPR2, PRI_11) callable from thread state, PendSV, and some IRQs
    ((uint8_t*) SCB.ADDR)[0x1F] = 0xC0;

    [[maybe_unused]] auto n = os::fork(ptr, len, nullptr).mTag; verify(n == 0);

    // TODO always inits 1 kHz systick, should be optional & configurable
    verify(devices[0] != nullptr);
    systick::init(1, [](uint32_t) { devices[0]->irqTrigger(-1); });

    // TODO can't switch to unprivileged mode at this point:
    //  - fork calls init unprivileged, which can not reschedule (SCB access)
    //  - the ~Tasker destructor needs to switch back to single-stack mode
    //  - blinking an LED will need access to GPIO registers
    //  - use of the DWT cycle counter requires privileged mode
#if 0
    asm volatile (
        " mov r1,#3      \n"
        " msr control,r1 \n"
    ::: "r1");
#endif
}

Tasker::~Tasker () {
    asm volatile (
        " mov r1,sp      \n"
        " msr msp,r1     \n"
        " mov r1,#0      \n"
        " msr control,r1 \n"
    ::: "r1");
}

int Tasker::count;

void wrapper (Task* tp, void (*fun)(Message&,void*), void* arg) {
    debugf("`T<%d %x", tp->tid, tp);

    fun(*tp, arg);

    if (tp->mDst >= 0)
        os::put(*tp);

#if 1
    // determine stack high-water mark, i.e. lowest memory address used
    auto p = (uint32_t*) (tp+1);
    while (*p == 0xDDDD'DDDD)
        ++p;
    debugf("`T>%d %d", tp->tid, (uint32_t) p - (uint32_t) (tp+1));
#endif

    --Tasker::count;
    tp->deinit();
    tp->reschedule(Task::DEAD);
    verify(false); // never reached
}

Message& os::fork (uint32_t* ptr, uint16_t len, void (*fun)(Message&,void*), void* arg) {
    // prepare for high-water mark report when task ends
    ptr[0] = 0xDDDD'DDDD;
    duffs(ptr + 1, ptr, len - 1);

    // Task obj is placed at bottom of stack, top is passed on to new task
    auto tp = (Task*) ptr; // Task is not inited the normal way ...
    memset(tp, 0, sizeof *tp);

    auto sp = ptr + len;
    if (fun != nullptr) {
        sp -= 16;
        sp[8] = (uint32_t) tp;       // r0
        sp[9] = (uint32_t) fun;      // r1
        sp[10] = (uint32_t) arg;     // r2
//      sp[13] = 0;                  // lr
        sp[14] = (uint32_t) wrapper; // pc
        sp[15] = 0x0100'0000;        // psr
        ++Tasker::count;
    }

    tp->mDst = currTask;
    tp->mTag = tp->init(sp);
    tp->mLen = len;
    tp->mPtr = ptr;
    tp->mLnk = tp;

    tp->reschedule(Task::RUN);
    return *tp;
}

// void os::put (Message& msg)
void svc_1 (uint32_t* args) {
    auto& msg = *(Message*) args[0];
    verify(!msg.inUse());
    if (msg.mDst >= 0) {
        auto tp = tasks[msg.mDst];
        msg.mDst = currTask;
        tp->submit(msg);
    } else {
        auto dp = devices[~msg.mDst];
        verify(dp != nullptr);
        msg.mDst = currTask;
        dp->start(msg);
    }
}

// Message& os::get ()
void svc_2 (uint32_t* args) {
    args[0] = (uint32_t) &tasks[currTask]->wait();
}

//void os::revoke (Message& msg, int id)
void svc_3 (uint32_t* args) {
    auto& msg = *(Message*) args[0];
    int8_t id = args[1];
    if (msg.inUse()) { // it could have ended just before this call
        if(id >= 0)
            tasks[id]->remove(msg);
        else
            devices[~id]->abort(msg);
    }
}

#pragma GCC diagnostic ignored "-Wreturn-type"
#define NO_INLINE __attribute__((noinline))
NO_INLINE void os::put (Message&)         { asm ("svc 1"); }
NO_INLINE Message& os::get ()             { asm ("svc 2"); }
NO_INLINE void os::revoke (Message&, int) { asm ("svc 3"); }
#pragma GCC diagnostic pop

Message* os::call (Message& msg) {
    auto to = msg.mDst;
    put(msg);
    auto& reply = get();
    if (&reply == &msg)
        return nullptr;
    revoke(msg, to);
    verify(!msg.inUse());
    return &reply;
}

Message* os::delay (uint16_t ms) {
    Message msg {-1, 0, ms};
    auto mp = call(msg);
    if (mp != nullptr) { // stop the timer, another msg came in
        revoke(msg, -1);
        verify(!msg.inUse());
    }
    return mp;
}

int Task::init (uint32_t* top) {
    sp = top;

    for (auto& e : tasks)
        if (e == nullptr) {
            e = this;
            tid = &e - tasks;
            return tid;
        }

    verify(false); // no free task slot
    return -1;
}

void Task::deinit () {
    tasks[tid] = nullptr;
}

void Task::submit (Message& msg) {
    if (state == WAIT) {
        verify(cHead == nullptr);
        if (tid == currTask) { // stash r0, which was saved at the top of PSP
            uint32_t* p;
            asm ("mrs %0,psp" : "=r" (p));
            p[0] = (uint32_t) &msg;
        } else { // we must be in PendSV, r0 is now 8 saved regs down
            // TODO 8 is not correct when FPU_USED is set
            sp[8] = (uint32_t) &msg;
        }
        reschedule(RUN);
    } else
        append(msg);
}

Message& Task::wait () {
    auto mp = pull();
    if (mp == nullptr) {
        reschedule(state | WAIT); // result will be filled in later by submit()
        mp = (Message*) 0xDEADBEEF;
    }
    return *mp;
}

void Task::reschedule (int newState) {
    state = newState;
    if (nextTask < tid)
        nextTask = tid;
    while (true) {
        auto tp = tasks[nextTask];
        if (tp != nullptr && tp->state == RUN) {
            if (SCB[0x10](1)) {
                SCB[0x10](1) = 0; // ~SLEEPONEXIT
                debugf("`tz");
            }
            if (nextTask != currTask)
                SCB[0x04] = 1<<28; // ICSR = PENDSVSET
            break;
        }
        if (nextTask == 0) {
            SCB[0x10](1) = 1; // SLEEPONEXIT
            debugf("`tZ");
            break;
        }
        --nextTask;
    }
}

void Task::show () {
    printf("%c[%02d] %02x %010p %010p\n",
            tid == currTask ? '>' : ' ', tid, state, sp, cHead);
}

void os::showTasks () {
    printf(" -ID- ST ----SP---- ---WORK---\n");
    for (auto e : tasks)
        if (e != nullptr)
            e->show();
}

uint32_t* switcher (uint32_t* sp) {
#if STM32L0
    uint32_t p; { BlockIRQ irq; p = pending; pending = 0; }
#else
    auto p = __atomic_exchange_n(&pending, 0, __ATOMIC_RELAXED);
#endif
    while (p != 0) {
        auto i = __builtin_ctz(p); // gcc can count trailing zeros
        p &= ~(1<<i);
        devices[i]->finish();
    }

    if (nextTask != currTask) {
        debugf("`t=%d", nextTask);
        tasks[currTask]->sp = sp;
        currTask = nextTask;
        sp = tasks[currTask]->sp;
    }
    return sp;
}

extern "C" __attribute__((naked))
void PendSV_Handler () {
    asm (
        " mrs      r0,psp        \n"
#if STM32L0
        " sub      r0,#32        \n"
        " mov      r1,r0         \n"
        " stmia    r1!,{r4-r7}   \n"
        " mov      r4,r8         \n"
        " mov      r5,r9         \n"
        " mov      r6,r10        \n"
        " mov      r7,r11        \n"
        " stmia    r1!, {r4-r7}  \n"
#else
#if FPU_USED
        " tst      lr,#0x10      \n"
        " it       eq            \n"
        " vstmdbeq r0!,{s16-s31} \n"
#endif
        " stmdb    r0!,{r4-r11}  \n"
#endif
        " mov      r4,lr         \n"
        " blx      %0            \n"
        " mov      lr,r4         \n"
#if STM32L0
        " add      r0,#16        \n"
        " ldmia    r0!,{r4-r7}   \n"
        " mov      r8,r4         \n"
        " mov      r9,r5         \n"
        " mov      r10,r6        \n"
        " mov      r11,r7        \n"
        " mov      r1,r0         \n"
        " sub      r1,#32        \n"
        " ldmia    r1!,{r4-r7}   \n"
#else
        " ldmia    r0!,{r4-r11}  \n"
#if FPU_USED
        " tst      lr,#0x10      \n"
        " it       eq            \n"
        " vldmiaeq r0!,{s16-s31} \n"
#endif
#endif
        " msr      psp,r0        \n"
        " bx       lr            \n"
    :: "r" (switcher));
}

void (*const svcVec[])(uint32_t*) = {
    +[](uint32_t* args) { args[0] = -1; },
    svc_1,
    svc_2,
    svc_3,
};

extern "C" __attribute__((naked))
void SVC_Handler () {
    asm (
#if STM32L0
        " mov    r0,lr  \n"
        " mov    r1,#4  \n"
        " tst    r0,r1  \n"
        " bne    1f     \n"
        " mrs    r0,msp \n"
        " b      2f     \n"
        "1:             \n"
        " mrs    r0,psp \n"
        "2:             \n"
#else
        " tst    lr,#4  \n"
        " ite    eq     \n"
        " mrseq  r0,msp \n"
        " mrsne  r0,psp \n"
#endif

    // auto req = ((uint8_t*) args[6])[-2];
    // if (req >= sizeof svcVec / sizeof *svcVec)
    //     req = 0;
    // svcVec[req](args);

        " ldr    r3,[r0, #24]       \n"
        " ldr    r2,=%0             \n"
#if STM32L0
        " sub    r3,#2              \n"
        " ldrb   r3,[r3]            \n"
#else
        " ldrb.w r3,[r3, #-2]       \n"
#endif
        " cmp    r3,%1              \n"
#if STM32L0
        " bcc    1f                 \n"
        " mov    r3,#0              \n"
        "1:                         \n"
        " lsl    r3,#2              \n"
        " ldr    r3,[r2,r3]         \n"
#else
        " it     cs                 \n"
        " movcs  r3,#0              \n"
        " ldr.w  r3,[r2,r3,lsl #2]  \n"
#endif
        " bx     r3                 \n"
    :: "i" (svcVec), "i" (sizeof svcVec / sizeof *svcVec));
}

void failAt ([[maybe_unused]] char const* f, [[maybe_unused]] int l) {
    debugf("failAt %s:%d\n", f, l);
    msBusy(5000);
    systemReset(); // TODO or use endless loop with watchdog kicking in?
}

void hardFaultHandler (uint32_t* sp) {
    enum { CFSR=0x28, HFSR=0x2C, MMAR=0x34, BFAR=0x38 };

    uint32_t hfsr = SCB[HFSR], cfsr = SCB[CFSR],
             bfar = SCB[BFAR], mmar = SCB[MMAR];

    printf("[Hard Fault]  SP=%08x  HFSR=%08x  CFSR=%08x\n",
            sp, hfsr, cfsr);
    if (hfsr & (1<<30)) {
        if (cfsr & 0xFFFF0000)
            printf("  Usage fault %04x\n", cfsr >> 16);
        if (cfsr & 0xFF00) {
            printf("  Bus fault %02x\n", (uint8_t) (cfsr >> 8));
            if (cfsr & (1<<15))
                printf("    BFAR %08x\n", bfar);
        }
        if (cfsr & 0xFF) {
            printf("  Memory fault %02x\n", (uint8_t) cfsr);
            if (cfsr & (1<<7))
                printf("    MMAR %08x\n", mmar);
        }      
    }

    printf("\t R0=%08x  R1=%08x  R2=%08x  R3=%08x\n",
            sp[0], sp[1], sp[2], sp[3]);
    printf("\tR12=%08x  LR=%08x  PC=%08x PSR=%08x\n",
            sp[4], sp[5], sp[6], sp[7]);

    asm ("cpsid i"); // disable all interrupts
    while (true) {}
}

extern "C" __attribute__((naked))
void HardFault_Handler () {
    asm volatile (
#if STM32L0
        " mov    r0,lr  \n"
        " mov    r1,#4  \n"
        " tst    r0,r1  \n"
        " bne    1f     \n"
        " mrs    r0,msp \n"
        " b      2f     \n"
        "1:             \n"
        " mrs    r0,psp \n"
        "2:             \n"
        " bx    %0      \n"
    :: "r" (hardFaultHandler));
#else
        " tst   lr, #4  \n"
        " ite   eq      \n"
        " mrseq r0, msp \n"
        " mrsne r0, psp \n"
        " b     %0      \n"
    :: "i" (hardFaultHandler));
#endif
}

} // namespace jeeh
