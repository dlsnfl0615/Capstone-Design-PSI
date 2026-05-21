#include "hashing.h"
#include <iostream>
#include <bitset>
#include <thread>
#include <mutex>
#include <algorithm>

void SenderHashing::locate(const vector<string>& data) {
    const size_t data_size = data.size();

    if (data_size == 0) {
        cout << "[hashing] Sender data is empty.\n\n";
        return;
    }

    // 사용할 thread 개수 설정
    unsigned int num_threads = 8;

    // hardware_concurrency()가 0을 반환하는 경우 대비
    if (num_threads == 0) {
        num_threads = 8;
    }

    // 데이터 개수보다 thread가 많을 필요는 없음
    num_threads = min<unsigned int>(num_threads, static_cast<unsigned int>(data_size));

    cout << "[hashing] Sender parallel hashing start. threads = "
         << num_threads << ", data_size = " << data_size << endl;

    // bin별 lock
    vector<mutex> bin_locks(m);

    // 각 thread가 처리할 함수
    auto worker = [&](size_t start, size_t end, int thread_id) {
        for (size_t idx = start; idx < end; idx++) {
            const string& record = data[idx];

            // 1. 데이터 파싱 (PID 13자리 + 질병코드 10비트)
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

                // 같은 bin에 여러 thread가 동시에 접근하지 못하게 lock
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
                    cerr << "[Warning] Bin " << loc
                         << " is full! Item dropped." << endl;
                }
            }
        }
    };

    vector<thread> threads;
    threads.reserve(num_threads);

    size_t chunk_size = (data_size + num_threads - 1) / num_threads;

    for (unsigned int t = 0; t < num_threads; t++) {
        size_t start = t * chunk_size;
        size_t end = min(start + chunk_size, data_size);

        if (start >= end) {
            break;
        }

        threads.emplace_back(worker, start, end, static_cast<int>(t));
    }

    for (auto& th : threads) {
        th.join();
    }

    cout << "[hashing] Sender parallel Hashing complete. Data distributed into "
         << m << " bins.\n\n";
}