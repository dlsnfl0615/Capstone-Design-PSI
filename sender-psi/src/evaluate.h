#pragma once

#include <iostream>
#include <vector>
#include <map>
#include "parameters.h"
#include "seal/seal.h"
#include "mod.h"

using namespace std;
using namespace seal;

class SenderEvaluate {
public:
    vector<vector<vector<uint64_t>>> partitioning(
        const vector<vector<uint64_t>> hash_table
    );

    vector<vector<vector<uint64_t>>> get_coeffs(
        const vector<vector<vector<uint64_t>>> partitions
    );

    vector<uint64_t> compute_bin_coeffs(
        const vector<uint64_t>& roots,
        uint64_t plain_modulus
    );

    vector<vector<vector<uint64_t>>> extract_all_coefficients(
        const vector<vector<vector<uint64_t>>>& partitions,
        uint64_t plain_modulus
    );

    // 변경 전:
    //   map<int, Ciphertext>
    //
    // 변경 후:
    //   map<int, vector<Ciphertext>>
    //
    // 이유:
    // receiver가 exponent e에 대해 ciphertext 하나가 아니라
    // block별 ciphertext 여러 개를 보내기 때문.
    //
    // all_powers[k][b]는
    //   k승 암호문 중 b번째 block
    // 을 의미함.
    map<int, vector<Ciphertext>> make_all_powers(
        const map<int, vector<Ciphertext>> received_powers,
        Evaluator& evaluator,
        RelinKeys& relin_keys
    );

    // product도 block별로 수행해야 함.
    //
    // 기존:
    //   partition 하나당 ciphertext 1개
    //
    // 변경:
    //   partition 하나당 num_blocks개 ciphertext
    //
    // 결과 vector 순서:
    //   partition 0, block 0
    //   partition 0, block 1
    //   partition 1, block 0
    //   partition 1, block 1
    //   ...
    vector<Ciphertext> product(
        const map<int, vector<Ciphertext>> all_powers,
        const vector<vector<vector<uint64_t>>> coeffs,
        BatchEncoder& batch_encoder,
        Evaluator& evaluator,
        SEALContext& context
    );
};
