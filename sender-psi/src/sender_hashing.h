#pragma once

#include <vector>
#include <string>
#include "parameters.h"
#include "murmur3.h"
using namespace std;

class SenderHashing {
public:
    vector<vector<uint64_t>> hash_table = vector<vector<uint64_t>>(m, vector<uint64_t>(B, SENDER_DUMMY));
    
    inline uint32_t get_hash(uint64_t val, int func_idx) {
        static const uint32_t seeds[] = {
            0x12345678, 0x87654321, 0xDEADBEEF, 0xCAFEBABE
        };
        uint32_t out = 0;
        MurmurHash3_x86_32(&val, sizeof(uint64_t), seeds[func_idx], &out);
        return out;
    }
    void locate(const vector<string> data);
};