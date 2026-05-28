#pragma once

#include <cstdint>
#include <string>

using namespace std;

struct SenderCacheMeta {
    int m_value;
    int B_value;
    int h_value;
    int n_value;
    int num_blocks_value;
    int alpha_value;
    int B_prime_value;
    int seed_value;

    uint64_t sigma_value;
    uint64_t sender_dummy_value;
    uint64_t receiver_dummy_value;
    uint64_t plain_modulus_value;

    string table_file;
    string coeffs_file;
    string created_at;
};

SenderCacheMeta make_current_sender_meta(
    const string& table_file,
    const string& coeffs_file,
    uint64_t plain_modulus
);

void save_sender_meta(
    const SenderCacheMeta& meta,
    const string& filepath
);

SenderCacheMeta load_sender_meta(
    const string& filepath
);

void validate_sender_meta(
    const SenderCacheMeta& meta,
    uint64_t current_plain_modulus
);