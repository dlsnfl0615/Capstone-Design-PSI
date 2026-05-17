#pragma once

#include <iostream>
#include <vector>
#include <math.h>
#include <map>
#include "parameters.h"
#include "seal/seal.h"
#include "mod.h"

using namespace std;
using namespace seal;

class Windowing {
public:
    vector<int> get_exponents();

    // 기존:
    // map<int, Ciphertext>
    //
    // 변경 이유:
    // m <= n이면 exponent e에 대해 ciphertext 1개면 충분했음.
    // 하지만 m > n이면 전체 hash_table을 한 ciphertext에 담을 수 없음.
    // 따라서 exponent e마다 block별 ciphertext가 필요함.
    //
    // 예:
    // m = 32768, n = 16384이면 num_blocks = 2
    // result[e][0] -> bin 0 ~ 16383의 e승 암호문
    // result[e][1] -> bin 16384 ~ 32767의 e승 암호문
    map<int, vector<Ciphertext>> receiver_windowing(
        BatchEncoder& batch_encoder,
        Encryptor& encryptor,
        const uint64_t plain_modulus,
        const vector<int>& exponents,
        const vector<uint64_t>& hash_table);
};
