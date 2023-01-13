#include <jee.h>

using namespace jeeh;

int main () {
    printf("%s: i2c @ %u MHz\n", SVDNAME, SystemCoreClock/1'000'000);

    I2cGpio i2c;

    i2c.init("B9,B8");
    i2c.detect(); // look for audio codec

    i2c.init("H8,A8");
    i2c.detect(); // look for touch panel
}
