// the bare minimum: launch the task switcher, dump some info, done

#include <jee.h>
using namespace jeeh;

int main () {
    //fastClock();
    printf("\n### %s: min @ %d MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    uint32_t stack [250];
    Tasker tasker (stack);

    os::showTasks();

    while (tasker.count > 0)
        os::get();

    printf("done\n");
}
