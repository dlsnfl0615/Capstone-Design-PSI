#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include "parameters.h"
#include "data_loader.h"
#include "receiver_hashing.h"
#include "sender_hashing.h"
#include "windowing.h"
#include "evaluate.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

int main() {
    
    cout << "\n--- Receiver: Final Intersection Check ---" << endl;

    // [receiver result] 교집합 결과 확인
    // 복호화된 교집합 패킹 값들을 저장할 셋 (중복 제거)
    set<uint64_t> intersection_packed_values;

    int count = 0;
    for (size_t p = 0; p < producted.size(); p++) {
        // 암호문 복호화 및 디코딩 수행
        Plaintext plain_result;
        decryptor.decrypt(producted[p], plain_result);
        
        vector<uint64_t> decoded_slots;
        batch_encoder.decode(plain_result, decoded_slots);

        // 각 슬롯을 검사하여 0인 위치의 패킹된 값을 수집함
        for (int i = 0; i < m; i++) {
            // 수학적으로 P(y) = 0 이면 교집합임

            if (decoded_slots[i] == 0) {
                uint64_t packed_val = receiver_hashing.hash_table[i];
                count++;

                // 수신자의 더미 데이터가 아닌 실제 값인 경우에만 추가함
                if (packed_val != RECEIVER_DUMMY) {
                    intersection_packed_values.insert(packed_val);
                }
            }
        }
    }

    cout << "count: " << count << endl;

    // [receiver result] 원본 데이터와 대조하여 어떤 데이터가 교집합인지
    int found_count = 0;
    cout << "[Intersection Results]" << endl;
    
    for (const auto& record : receiver_data) {
        // 원본 레코드를 다시 패킹하여 비교 대상으로 만듦 (hashing 로직과 동일해야 함)
        string pid_str = record.substr(0, 13);
        string disease_str = record.substr(13, 10);
        uint64_t pid_val = std::stoull(pid_str);
        uint64_t disease_val = std::stoull(disease_str, nullptr, 2);
        uint64_t full_item = (pid_val << 10) | disease_val;
        uint64_t x_L = full_item >> 13;

        bool is_intersected = false;
        // h개의 가능한 해시 인덱스 중 하나라도 셋에 존재하면 교집합임
        for (int i = 0; i < h; i++) {
            uint64_t packed = (x_L << 2) | (static_cast<uint64_t>(i));
            if (intersection_packed_values.count(packed)) {
                is_intersected = true;
                break;
            }
        }

        if (is_intersected) {
            cout << "  [O] Found Intersection: PID(" << pid_str << ") Disease(" << disease_str << ")" << endl;
            found_count++;
        } else {
            // 필요 시 교집합이 없는 데이터도 출력 가능함
            // cout << "  [X] No Match: " << pid_str << endl;
        }
    }
    
    return 0;
}