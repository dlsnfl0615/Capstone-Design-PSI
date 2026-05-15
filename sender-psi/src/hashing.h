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
            0x12345678, 0x87654321, 0xDEADBEEF, 0xCAFEBABE, 0x34DA12B8 // 5번째는 초기 해싱을 위한 시드
        };
        uint32_t out = 0;
        MurmurHash3_x86_32(&val, sizeof(uint64_t), seeds[func_idx], &out);
        return out;
    }
    inline uint32_t get_hash(string val) {
        static const uint32_t seed = 0x4A3C01E2;
        uint32_t out = 0;
        MurmurHash3_x86_32(val.data(), static_cast<int>(val.size()), seed, &out);

        return out;
    }
    vector<uint32_t> compress(const vector<string> data);
    void locate(const vector<uint32_t> data);
};