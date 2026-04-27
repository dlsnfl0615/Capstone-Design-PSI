#pragma once

#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include "seal/seal.h"
#include "../parameters.h"

using namespace std;

class PsiSender {
public:
    map<uint64_t, seal::Ciphertext> received_powers;
    vector<vector<vector<uint64_t>>> partitioned_tables;
    vector<vector<seal::Plaintext>> batched_coeffs;
    vector<seal::Ciphertext> evaluation_results;

    void load_receiver_powers(const string& filename, seal::SEALContext& context);
    void partition_bins(const vector<vector<uint64_t>>& hash_table);
    void compute_coefficients(seal::BatchEncoder& encoder, uint64_t plain_modulus);
    void evaluate_polynomials(seal::Evaluator& evaluator, seal::RelinKeys& relin_keys);

    /**
     * @brief [Step 6] 결과에 무작위 값을 곱하고 응답 파일(response.bin)로 저장함
     */
    void randomize_and_save_responses(
        seal::Evaluator& evaluator, 
        seal::BatchEncoder& encoder, 
        uint64_t plain_modulus, 
        const string& filename);
};