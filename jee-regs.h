// interface definitions for hardware I/O register access, based on SVD

template< uint32_t A >
struct IoReg {
    enum { ADDR = A };

    static constexpr bool canBitBand (); // defined after "jee-svd.h" include

    struct Bits {
        uint32_t o;
        uint8_t b, w;

        inline operator int () const;
        inline void operator= (int v) const;

        void operator= (Bits const& v) const {
            operator= ((int) v);
        }

        constexpr auto bitBandAddr () const {
            return (A&0xF000'0000) + 0x0200'0000 + ((A+o)<<5) + (b<<2);
        }
    };

    struct Word {
        uint32_t o;

        constexpr auto operator() (uint8_t bit, uint8_t width =1) const {
            return Bits{ o+4*(bit/32), (uint8_t) (bit%32), width };
        }

        operator int () const {
            return *(volatile uint32_t*) (A+o);
        }

        void operator= (int v) const {
            *(volatile uint32_t*) (A+o) = v;
        }

        void operator= (Word const& v) const {
            operator= ((int) v);
        }
    };

    constexpr auto operator[] (uint32_t off) const {
        return Word{ off };
    }

    constexpr auto operator() (uint32_t bit, uint8_t width) const {
        return operator[](4*(bit/32))(bit%32, width);
    }
};

#include "jee-svd.h"

template< uint32_t A >
constexpr bool IoReg<A>::canBitBand () {
#if STM32F1 | STM32F4 | STM32L4
    return (A>>20) == 0x200 || (A>>20) == 0x400;
#else
    return false;
#endif
}

template< uint32_t A >
IoReg<A>::Bits::operator int () const {
    if (canBitBand() && w == 1) {
        auto ptr = (volatile uint32_t*) bitBandAddr();
        return *ptr;
    } else {
        auto mask = (1<<w)-1;
        auto ptr = (volatile uint32_t*) (A+o);
        return (*ptr>>b) & mask;
    }
}

template< uint32_t A >
void IoReg<A>::Bits::operator= (int v) const {
    if (canBitBand() && w == 1) {
        auto ptr = (volatile uint32_t*) bitBandAddr();
        *ptr = v;
    } else {
        auto mask = (1<<w)-1;
        auto ptr = (volatile uint32_t*) (A+o);
        *ptr = (*ptr & ~(mask<<b)) | ((v&mask)<<b);
    }
}
