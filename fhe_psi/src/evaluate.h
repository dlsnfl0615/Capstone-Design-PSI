#pragma once

#include <iostream>
#include <vector>
#include "parameters.h"

using namespace std;

class SenderEvaluate {
public:
    vector<vector<vector<uint64_t>>> partitioning(const vector<vector<uint64_t>> hash_table);
    vector<vector<vector<uint64_t>>> get_coeffs(const vector<vector<vector<uint64_t>>> partitions);
    vector<uint64_t> compute_bin_coefficients(const vector<uint64_t>& roots, uint64_t plain_modulus);
    vector<vector<vector<uint64_t>>> extract_all_coefficients(
        const vector<vector<vector<uint64_t>>>& partitions, 
        uint64_t plain_modulus);
};