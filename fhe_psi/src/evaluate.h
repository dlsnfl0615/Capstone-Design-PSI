#pragma once

#include <iostream>
#include <vector>
#include <map>
#include "parameters.h"
#include "seal/seal.h"

using namespace std;
using namespace seal;

class SenderEvaluate {
public:
    vector<vector<vector<uint64_t>>> partitioning(const vector<vector<uint64_t>> hash_table);
    vector<vector<vector<uint64_t>>> get_coeffs(const vector<vector<vector<uint64_t>>> partitions);
    vector<uint64_t> compute_bin_coefficients(const vector<uint64_t>& roots, uint64_t plain_modulus);
    vector<vector<vector<uint64_t>>> extract_all_coefficients(
        const vector<vector<vector<uint64_t>>>& partitions, 
        uint64_t plain_modulus);
    map<int, Ciphertext> make_all_powers(
        const map<int, Ciphertext> received_powers,
        Evaluator& evaluator,
        RelinKeys& relin_keys);
    vector<Ciphertext> intersect(
        const map<int, Ciphertext> all_powers,
        const vector<vector<vector<uint64_t>>> coeffs,
        BatchEncoder& batch_encoder,
        Evaluator& evaluator,
        SEALContext& context);
};