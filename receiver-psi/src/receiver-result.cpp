#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <fstream>
#include "parameters.h"
#include "data_loader.h"
#include "hashing.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

int main() {
    cout << "[load] SEAL objects loading..";

    // receiver 원본 데이터 다시 로드
    auto receiver_data = load_receiver("data/B_receiver_5300.csv");

    // parms 로드
    EncryptionParameters parms;
    ifstream parms_in("../data/parms.bin", ios::binary);
    if (!parms_in.is_open()) {
        cerr << "Error: parms.bin 파일을 찾을 수 없습니다." << endl;
        return 1;
    }
    parms.load(parms_in);
    parms_in.close();

    // request와 같은 context 생성
    SEALContext context(parms);

    // 공개키 로드
    PublicKey public_key;
    ifstream pk_in("../data/public_key.bin", ios::binary);
    if (pk_in.is_open()) {
        public_key.load(context, pk_in);
        pk_in.close();
    }

    // relin 키 로드
    RelinKeys relin_keys;
    ifstream rk_in("../data/relin_key.bin", ios::binary);
    if (rk_in.is_open()) {
        relin_keys.load(context, rk_in);
        rk_in.close();
    }

    // 비밀키 로드
    SecretKey secret_key;
    ifstream sk_in("../data/secret_key.bin", ios::binary);
    if (sk_in.is_open()) {
        secret_key.load(context, sk_in);
        sk_in.close();
    }

    // receiver 해시 테이블 로드
    vector<uint64_t> hash_table;
    ifstream hash_in("../data/receiver_hash.bin", ios::binary);
    if (hash_in.is_open()) {
        size_t table_size;
        hash_in.read(reinterpret_cast<char*>(&table_size), sizeof(size_t));
        hash_table.resize(table_size);
        hash_in.read(reinterpret_cast<char*>(hash_table.data()), table_size * sizeof(uint64_t));
        hash_in.close();
    }

    // 복호화 객체, 배칭 객체 생성
    Decryptor decryptor(context, secret_key);
    BatchEncoder batch_encoder(context);

    // sender가 보내준 다항식 연산 결과 로드
    vector<Ciphertext> producted;
    ifstream res_in("../data/result.bin", ios::binary);
    if (res_in.is_open()) {
        size_t result_size;
        res_in.read(reinterpret_cast<char*>(&result_size), sizeof(size_t));

        for (size_t i = 0; i < result_size; i++) {
            Ciphertext ct;
            ct.load(context, res_in);
            producted.push_back(move(ct));
        }
        res_in.close();
    }
    cout << "completed.\n\n";

    cout << "Final intersection checking..";

    // 교집합 결과 확인
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
                uint64_t packed_val = hash_table[i];
                count++;

                // 수신자의 더미 데이터가 아닌 실제 값인 경우에만 추가함
                if (packed_val != RECEIVER_DUMMY) {
                    intersection_packed_values.insert(packed_val);
                }
            }
        }
    }
    cout << "completed.\n\n";
    
    cout << "count: " << count << endl;

    // [receiver result] 원본 데이터와 대조하여 어떤 데이터가 교집합인지
    ofstream intersection_out("../data/intersections.csv");
    int found_count = 0;
    ReceiverHashing receiver_hashing;
    cout << "Receiver: Intersection Results is saved in \"intersections.csv\"" << endl;
    
    size_t total_records = receiver_data.size(); // 전체 데이터 개수
    size_t processed_records = 0; // 처리된 데이터 개수
    size_t report_interval = total_records / 10 == 0 ? 1 : total_records / 10; // 10% 단위 설정함
    int shift_bits = static_cast<int>(std::log2(m)); // m 기반 시프트 비트 계산함

    for (const auto& record : receiver_data) {
        // csv 저장을 위해 원본 문자열에서 pid와 질병코드를 분리함
        string pid_str = record.substr(0, 13);
        string disease_str = record.substr(13, 10);

        // [핵심] compress 함수와 동일하게 원본 문자열 전체를 32비트 MurmurHash로 해싱함
        uint32_t hashed_item = receiver_hashing.get_hash(record);

        // 32비트 해싱 결과물에서 상위 비트 분리함 (locate 함수의 x_L 분리 로직과 일치)
        uint64_t x_L = hashed_item >> shift_bits;

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
            intersection_out << pid_str << "," << disease_str << endl;
            found_count++;
        }

        // 작업률 디버깅 출력함
        processed_records++;
        if (processed_records % report_interval == 0 || processed_records == total_records) {
            double progress = (static_cast<double>(processed_records) / total_records) * 100.0;
            std::cout << "[Intersection Progress] " << progress << "% (" << processed_records << "/" << total_records << ")\n";
        }
    }
    
    return 0;
}