#pragma once

#include <iostream>
using namespace std;

uint64_t safe_mul_mod(uint64_t a, uint64_t b, uint64_t m);

// 오버플로우 방지 거듭제곱 함수
uint64_t power_mod(uint64_t base, uint64_t exp, uint64_t mod);