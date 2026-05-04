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
    map<int, Ciphertext> receiver_windowing(
        BatchEncoder& batch_encoder,
        Encryptor& encryptor,
        const uint64_t plain_modulus,
        const vector<int> exponents,
        const vector<uint64_t> hash_table);
};