#include "mod.h"

uint64_t safe_mul_mod(uint64_t a, uint64_t b, uint64_t m) {
    uint64_t res = 0;
    a %= m;
    while (b > 0) {
        if (b % 2 == 1) res = (res + a) % m;
        a = (a + a) % m;
        b /= 2;
    }
    return res;
}

// 오버플로우 방지 거듭제곱 함수 
uint64_t power_mod(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t res = 1;
    base %= mod;
    while (exp > 0) {
        if (exp % 2 == 1) res = safe_mul_mod(res, base, mod);
        base = safe_mul_mod(base, base, mod);
        exp /= 2;
    }
    return res;
}