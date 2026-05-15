#include "hashing.h"
#include <iostream>
#include <bitset>

vector<uint32_t> SenderHashing::compress(const vector<string> data) {
    vector<uint32_t> result;

    for (const string record : data) {
        uint32_t compressed = get_hash(record);
        result.push_back(compressed);
    }

    return result;
}


void SenderHashing::locate(const vector<uint32_t> data) {
    for (const auto& record : data) {
        uint64_t full_item = record;
        int shift_bits = static_cast<int>(std::log2(m));
        uint64_t mask = (1ULL << shift_bits) - 1;

        // 2. 순열 기반 해싱 비트 분리 (하위 13비트 x_R, 상위 41비트 x_L)
        uint64_t x_R = full_item & mask; 
        uint64_t x_L = full_item >> static_cast<int>(log2(m));

        // 3. 단순 해싱: h개의 모든 해시 위치에 아이템 삽입
        for (int i = 0; i < h; i++) {
            // 위치 계산: Loc = (H_i(x_L) % m) ^ x_R
            int loc = (get_hash(x_L, i) % m) ^ x_R;
            
            // 수신자와 동일한 포맷으로 패킹 (x_L + hash_idx)
            uint64_t packed = (x_L << 2) | (static_cast<uint64_t>(i));

            // 빈(Bin)의 빈자리를 찾아 삽입 (최대 B개)
            bool inserted = false;
            for (int slot = 0; slot < B; slot++) {
                if (hash_table[loc][slot] == SENDER_DUMMY) {
                    hash_table[loc][slot] = packed;
                    inserted = true;
                    break;
                }
            }

            if (!inserted) {
                // 특정 빈이 가득 찬 경우 (B=74를 넘는 충돌 발생 시)
                std::cerr << "[Warning] Bin " << loc << " is full! Item dropped." << std::endl;
            }
        }
    }
    cout << "[hashing]Sender simple Hashing complete. Data distributed into " << m << " bins.\n\n";
}
