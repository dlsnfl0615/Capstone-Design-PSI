#include "hashing.h"
#include <iostream>
#include <bitset>

void SimpleHashing::locate(const vector<string> data) {
    for (const auto& record : data) {
        // 1. 데이터 파싱 (PID 13자리 + 질병코드 10비트)
        string pid_str = record.substr(0, 13);
        string disease_str = record.substr(13, 10);

        uint64_t pid_val = std::stoull(pid_str);
        uint64_t disease_val = std::stoull(disease_str, nullptr, 2);
        uint64_t full_item = (pid_val << 10) | disease_val;

        // 2. 순열 기반 해싱 비트 분리 (하위 13비트 x_R, 상위 41비트 x_L)
        uint64_t x_R = full_item & 0x1FFF; 
        uint64_t x_L = full_item >> 13;

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
    cout << "Sender: Simple Hashing complete. Data distributed into " << m << " bins." << endl;
}