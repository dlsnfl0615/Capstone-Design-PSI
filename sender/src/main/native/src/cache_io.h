#pragma once

#include <cstdint>
#include <string>
#include <vector>

using namespace std;

void save_hash_table_bin(
    const vector<vector<uint64_t>>& table,
    const string& filepath
);

vector<vector<uint64_t>> load_hash_table_bin(
    const string& filepath
);

void save_coeffs_bin(
    const vector<vector<vector<uint64_t>>>& coeffs,
    const string& filepath
);

vector<vector<vector<uint64_t>>> load_coeffs_bin(
    const string& filepath
);