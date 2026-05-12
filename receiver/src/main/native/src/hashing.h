#pragma once

#include <iostream>
#include <cstdint>
#include <vector>
#include <string>
#include <bitset>
#include <map>
#include <cmath>
#include "parameters.h"
#include "murmur3.h"

using namespace std;

struct RestoredData {
    string pid;      // 주민등록번호 13자리
    string disease;  // 질병코드 10비트 (이진 문자열)
};

class ReceiverHashing {
public:
    vector<uint64_t> hash_table = vector<uint64_t>(m, RECEIVER_DUMMY); // m=8192

    inline uint32_t get_hash(uint64_t val, int func_idx) {
        static const uint32_t seeds[] = {
            0x12345678, 0x87654321, 0xDEADBEEF, 0xCAFEBABE
        };
        uint32_t out = 0;
        MurmurHash3_x86_32(&val, sizeof(uint64_t), seeds[func_idx], &out);
        return out;
    }
    void locate(const vector<string> data);
    vector<RestoredData> restore_original_data();
    void print_hash_table();
};
