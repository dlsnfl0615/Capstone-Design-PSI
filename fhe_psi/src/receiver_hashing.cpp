#include "receiver_hashing.h"

void ReceiverHashing::locate(const vector<string> data) {
    for (const auto& record : data) {
        string pid_str = record.substr(0, 13);
        string disease_str = record.substr(13, 10);

        uint64_t pid_val = std::stoull(pid_str); 
        uint64_t disease_val = std::stoull(disease_str, nullptr, 2); 
        uint64_t full_item = (pid_val << 10) | disease_val; 

        // 순열 기반 해싱을 위한 비트 분리
        uint64_t x_R = full_item & 0x1FFF; // 하위 13비트 (log2 8192)
        uint64_t x_L = full_item >> 13;   // 상위 41비트
        
        // 뻐꾸기 해싱 초기 설정
        int hash_idx = 0; // 0번 해시 함수부터 시작 (hashing.h의 0-based index 기준)
        uint64_t packed = (x_L << 2) | (static_cast<uint64_t>(hash_idx));
        
        // 초기 위치 계산: Loc = (H(x_L) % m) ^ x_R
        int loc = (get_hash(x_L, hash_idx) % m) ^ x_R;

        int kicks = 0;
        bool inserted = false;

        // 뻐꾸기 해싱 루프: 빈자리를 찾거나 최대 킥 횟수에 도달할 때까지 반복
        while (kicks < CUCKOO_MAX_KICK) { // CUCKOO_MAX_KICK = 500
            // 해당 슬롯이 비어있으면 바로 저장하고 종료
            if (hash_table[loc] == RECEIVER_DUMMY) {
                hash_table[loc] = packed;
                inserted = true;
                break;
            }

            // 이미 주인이 있다면 밀어내고(Kick-out) 현재 아이템을 저장
            std::swap(packed, hash_table[loc]);
            kicks++;

            // 밀려난 아이템의 정보 복구 (XOR 연산의 가역성 활용) 
            uint64_t old_x_L = packed >> 2;
            int old_hash_idx = static_cast<int>(packed & 0x3);
            uint32_t old_h_val = get_hash(old_x_L, old_hash_idx);
            
            // x_R = Loc ^ (H(x_L) % M) 로 복구
            uint64_t restored_x_R = static_cast<uint64_t>(loc) ^ (old_h_val % m);

            // 다음 해시 함수 선택 (0 -> 1 -> 2 -> 3 -> 0 순환)
            int next_hash_idx = (old_hash_idx + 1) % h; // h = 4
            
            // 새로운 위치 계산 및 패킹 값 업데이트
            packed = (old_x_L << 2) | (static_cast<uint64_t>(next_hash_idx));
            loc = (get_hash(old_x_L, next_hash_idx) % m) ^ restored_x_R;
        }

        if (!inserted) {
            // 해싱 실패 시 처리
            std::cerr << "[Warning] Cuckoo hashing failed for record: " << record << std::endl;
        }
    }
}

vector<RestoredData> ReceiverHashing::restore_original_data() {
    vector<RestoredData> restored;
    
    for (int i = 0; i < hash_table.size(); i++) {
        if (hash_table[i] == RECEIVER_DUMMY || hash_table[i] == SENDER_DUMMY) continue;

        uint64_t packed = hash_table[i];
        int loc = i;
        int hash_idx = static_cast<int>(packed & 0x3);
        uint64_t x_L = packed >> 2;

        // XOR 역산을 통해 x_R 복구
        uint32_t h_val = get_hash(x_L, hash_idx);
        uint64_t x_R = static_cast<uint64_t>(loc) ^ (h_val % m);

        uint64_t full_item = (x_L << 13) | (x_R & 0x1FFF);
        
        RestoredData result;
        result.pid = std::to_string(full_item >> 10);
        result.disease = std::bitset<10>(full_item & 0x3FF).to_string();
        
        restored.push_back(result);
    }
    
    return restored;
}

void ReceiverHashing::print_hash_table() {
    for (int i = 0; i < hash_table.size(); i++) {
        if (hash_table[i] != RECEIVER_DUMMY) {
            cout << "loc: " << i << ", val: " << hash_table[i] << endl;
        }
    }
}
