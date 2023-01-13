#include <cstdint>
#include <cstring>

// Low-footprint assertions, i.e. enough info to identify the source line.
// See https://interrupt.memfault.com/blog/asserts-in-embedded-systems

#ifdef NVERIFY
#define verify(exp) ((void)0)
#else
#define verify(exp) \
    do if (!(exp)) jeeh::failAt(__FILE__, __LINE__); while (false)
#endif

// A macro which acts as compile-time adjustable output filter for printf.

#ifdef DEBUGF
#define debugf(fmt, ...) \
    do \
        if constexpr (fmt[0] != '`' || strchr(DEBUGF, fmt[1]) == nullptr) \
            printf(fmt "\n", ##__VA_ARGS__); \
    while (false)
#else
#define debugf(...)
#endif

extern "C" {
    extern uint32_t SystemCoreClock; // Hz, set in CMSIS startup
    int printf (char const* fmt ...);
}

namespace jeeh {

#include "jee-regs.h"
#include "jee-arm.h"
#include "jee-os.h"

} // namespace jeeh
