// interface to the message-based multi-tasking kernel

struct Message {
    int8_t   mDst =0;
    int8_t   mTag =0;
    uint16_t mLen =0;
    void*    mPtr =nullptr;
    Message* mLnk =this;

    bool inUse () const { return mLnk != this; }

    Message (const Message&) =delete;
    void operator= (const Message&) =delete;
};

struct Chain {
    bool isEmpty () const { return cHead == nullptr; }
    Message* first () const { return cHead; }
    bool insert (Message& msg);
    bool append (Message& msg);
    bool remove (Message& msg);
    Message* pull ();
protected:
    Message* cHead =nullptr;
};

struct Device {
    uint8_t devId;

    Device ();

    void irqInstall (int num) const;
    void irqTrigger (int num);
    void reply (Message*);

    virtual void start (Message&) =0;
    virtual bool interrupt (int) =0;
    virtual void finish () =0;
    virtual void abort (Message&) =0;

    static void nvicEnable (int num);
    static void nvicDisable (int num);
};

// TODO move Ticker definition here?

struct ExtIrq : Device, Chain {
    enum { NONE, RISE, FALL, BOTH };

    ExtIrq ();

    void start (Message& m) override;
    void finish () override;
    void abort (Message& m) override;
    bool interrupt (int irq) override;

private:
    uint32_t flags =0;
};

struct Tasker {
    Tasker (uint32_t* stack, uint32_t words);

    template< uint32_t N > // see os::fork comment
    Tasker (uint32_t (&stack)[N]) : Tasker (stack, N) {}

    ~Tasker ();

    static int count;
};

namespace os {
    Message& fork (uint32_t*, uint16_t, void (*)(Message&,void*), void* =nullptr);

    // when handed an array as stack, this variant will auto-derive its size
    template< uint32_t N >
    Message& fork (uint32_t (&s)[N], void (*f)(Message&,void*), void* a =nullptr) {
        return fork (s, N, f, a);
    }

    void put (Message& msg);
    Message& get ();
    void revoke (Message& msg, int dest);

    Message* call (Message& msg);
    Message* delay (uint16_t ms);

    void showTasks ();
}
