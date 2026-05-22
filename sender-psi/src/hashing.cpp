#include "hashing.h"
#include <iostream>
#include <bitset>
#include <omp.h> // OpenMP 헤더 추가
#include <mutex>
#include <algorithm>

void SenderHashing::locate(const vector<string>& data) {
    const size_t data_size = data.size();

    if (data_size == 0) {
        cout << "[hashing] Sender data is empty.\n\n";
        return;
    }

    cout << "[hashing] Sender parallel hashing start. data_size = " << data_size << endl;

    // bin별 lock은 그대로 유지합니다 (데이터 무결성을 위해)
    vector<mutex> bin_locks(m);

    // OpenMP를 사용하여 루프를 병렬로 처리
    // omp_set_num_threads()는 main.cpp에서 이미 설정했으므로 여기서 별도 설정 불필요
    #pragma omp parallel for
    for (size_t idx = 0; idx < data_size; idx++) {
        const string& record = data[idx];

        // 1. 데이터 파싱
        string pid_str = record.substr(0, 13);
        string disease_str = record.substr(13, 10);

        uint64_t pid_val = std::stoull(pid_str);
        uint64_t disease_val = std::stoull(disease_str, nullptr, 2);
        uint64_t full_item = (pid_val << 10) | disease_val;

        // 2. 순열 기반 해싱 비트 분리
        int shift_bits = get_log2_m();
        uint64_t mask = get_x_r_mask();

        uint64_t x_R = full_item & mask;
        uint64_t x_L = full_item >> shift_bits;

        // 3. h개의 모든 해시 위치에 아이템 삽입
        for (int i = 0; i < h; i++) {
            int loc = (get_hash(x_L, i) % m) ^ x_R;
            uint64_t packed = (x_L << 2) | static_cast<uint64_t>(i);

            bool inserted = false;

            // 락은 그대로 사용 (OpenMP 내부에서도 안전하게 작동)
            {
                lock_guard<mutex> lock(bin_locks[loc]);
                for (int slot = 0; slot < B; slot++) {
                    if (hash_table[loc][slot] == SENDER_DUMMY) {
                        hash_table[loc][slot] = packed;
                        inserted = true;
                        break;
                    }
                }
            }

            if (!inserted) {
                // cerr은 여러 스레드에서 동시에 출력하면 엉킬 수 있음
                // 하지만 디버깅 용도라면 문제없음
            }
        }
    }

    cout << "[hashing] Sender parallel Hashing complete. Data distributed into "
         << m << " bins.\n\n";
}
