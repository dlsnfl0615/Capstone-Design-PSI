#pragma once

#include <vector>
#include <map>
#include "data_loader.h"
#include "../parameters.h"
#include "murmur3.h"

using namespace std;

struct RestoredData {
    std::string pid;      // 주민등록번호 13자리
    std::string disease;  // 질병코드 10비트 (이진 문자열)
};

class Hashing {
public:
    vector<uint64_t> hash_table = vector<uint64_t>(m, 0);

    inline uint32_t Hashing::get_hash(uint64_t val, int func_idx) {
        static const uint32_t seeds[] = {
            0x12345678, 0x87654321, 0xDEADBEEF, 0xCAFEBABE
        };
        uint32_t out = 0;
        MurmurHash3_x86_32(&val, sizeof(uint64_t), seeds[func_idx], &out);
        return out;
    }
    void locate(const vector<string> data);
    vector<uint64_t> batching(size_t slot_count);
    RestoredData restore_original_data(uint64_t packed, int loc);
    void print_hash_table();
};
