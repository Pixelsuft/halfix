#include "cpu/cpu.h"
#include "cpu/opcodes.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif

void bt16(uint16_t a, int shift){
    cpu_set_cf(a >> (shift & 15) & 1);
}
void bt32(uint32_t a, int shift){
    cpu_set_cf(a >> (shift & 31) & 1);
}
void bts16(uint16_t *a, int shift){
    shift &= 15;
    cpu_set_cf(*a >> shift & 1);
    *a |= 1 << shift;
}
void bts32(uint32_t *a, int shift){
    shift &= 31;
    cpu_set_cf(*a >> shift & 1);
    *a |= 1 << shift;
}
void btc16(uint16_t *a, int shift){
    shift &= 15;
    cpu_set_cf(*a >> shift & 1);
    *a ^= 1 << shift;
}
void btc32(uint32_t *a, int shift){
    shift &= 31;
    cpu_set_cf(*a >> shift & 1);
    *a ^= 1 << shift;
}
void btr16(uint16_t *a, int shift){
    shift &= 15;
    cpu_set_cf(*a >> shift & 1);
    *a &= ~(1 << shift);
}
void btr32(uint32_t *a, int shift){
    shift &= 31;
    cpu_set_cf(*a >> shift & 1);
    *a &= ~(1 << shift);
}

uint16_t bsf16(uint16_t src, uint16_t old){
    if (src) {
        cpu_set_zf(0);
#ifdef _MSC_VER
        unsigned long index;
        _BitScanForward(&index, src & 0xFFFF);
        return (uint16_t)index;
#else
        return __builtin_ctz(src & 0xFFFF);
#endif
    }else{
        cpu_set_zf(1);
        return old;
    }
}
uint32_t bsf32(uint32_t src, uint32_t old){
    cpu.laux = BIT;
    if(src){
        cpu.lr = 1; // Clear ZF
#ifdef _MSC_VER
        unsigned long index;
        _BitScanForward(&index, src);
        return index;
#else
        return __builtin_ctz(src);
#endif
    }else{
        cpu.lr = 0; // Assert ZF
        return old;
    }
}
uint16_t bsr16(uint16_t src, uint16_t old){
    if(src){
        cpu_set_zf(0);
#ifdef _MSC_VER
        unsigned long index;
        _BitScanReverse(&index, src & 0xFFFF);
        return (uint16_t)((31 - index) ^ 31);
#else
        return __builtin_clz(src & 0xFFFF) ^ 31;
#endif
    }else{
        cpu_set_zf(1);
        return old;
    }
}
uint32_t bsr32(uint32_t src, uint32_t old){
    if(src){
        cpu_set_zf(0);
#ifdef _MSC_VER
        unsigned long index;
        _BitScanReverse(&index, src);
        return (31 - index) ^ 31;
#else
        return __builtin_clz(src) ^ 31;
#endif
    }else{
        cpu_set_zf(1);
        return old;
    }
}