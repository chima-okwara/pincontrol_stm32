// simple demo of the basic multi-tasking setup in JeeH

#include <jee.h>
using namespace jeeh;

void demo (Message&, void*) {
    Pin led ("B3");
    led.mode("P");

    led = 1;
    os::delay(100);
    led = 0;
}

int main () {
    uint32_t stack [250];               // 1. start multi-tasking
    Tasker tasker (stack);

    uint32_t demoStack [50];            // 2. create a new task
    os::fork(demoStack, demo);

    while (tasker.count > 0)            // 3. wait for it to finish
        os::get();
}
