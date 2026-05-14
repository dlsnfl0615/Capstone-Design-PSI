#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <fstream>
#include <iomanip>
#include "parameters.h"
#include "data_loader.h"
#include "hashing.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

extern "C" {
#ifdef _WIN32
__declspec(dllexport) // 윈도우 환경 DLL 내보내기
#endif
int result(const char* storage_dir, const char* receiver_csv) {
    string sd(storage_dir);
    cout << "[load] SEAL objects loading..";

    // 로드 시간 측정 시작
    auto t_load_start = Clock::now();

    // receiver 원본 데이터 다시 로드
    auto receiver_data = load_receiver(receiver_csv);

    // parms 로드
    EncryptionParameters parms;
    ifstream parms_in(sd + "/parms.bin", ios::binary);
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
    ifstream pk_in(sd + "/public_key.bin", ios::binary);
    if (pk_in.is_open()) {
        public_key.load(context, pk_in);
        pk_in.close();
    }

    // relin 키 로드
    RelinKeys relin_keys;
    ifstream rk_in(sd + "/relin_key.bin", ios::binary);
    if (rk_in.is_open()) {
        relin_keys.load(context, rk_in);
        rk_in.close();
    }

    // 비밀키 로드
    SecretKey secret_key;
    ifstream sk_in(sd + "/secret_key.bin", ios::binary);
    if (sk_in.is_open()) {
        secret_key.load(context, sk_in);
        sk_in.close();
    }

    // receiver 해시 테이블 로드
    vector<uint64_t> hash_table;
    ifstream hash_in(sd + "/receiver_hash.bin", ios::binary);
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
    ifstream res_in(sd + "/result.bin", ios::binary);
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

    auto t_load_end = Clock::now();

    cout << "Final intersection checking..";

    // 복호화 시간 측정 시작
    auto t_decrypt_start = Clock::now();

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

    auto t_decrypt_end = Clock::now();

    cout << "completed.\n\n";
    cout << "count: " << count << endl;

    // 교집합 원본 데이터 대조 시간 측정 시작
    auto t_intersect_start = Clock::now();

    // [receiver result] 원본 데이터와 대조하여 어떤 데이터가 교집합인지
    ofstream intersection_out(sd + "/intersections.csv");
    int found_count = 0;
    cout << "Receiver: Intersection Results is saved in \"intersections.csv\"" << endl;
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
            intersection_out << pid_str << "," << disease_str << endl;
            found_count++;
        }
    }

    auto t_intersect_end = Clock::now();

    // C++ 단계별 소요 시간 저장
    double loadMs = chrono::duration<double, milli>(t_load_end - t_load_start).count();
    double decryptMs = chrono::duration<double, milli>(t_decrypt_end - t_decrypt_start).count();
    double intersectMs = chrono::duration<double, milli>(t_intersect_end - t_intersect_start).count();

    ofstream timing_out(sd + "/cpp_timing.json");
    timing_out << fixed << setprecision(3)
               << "{\"loadMs\":" << loadMs
               << ",\"decryptMs\":" << decryptMs
               << ",\"intersectMs\":" << intersectMs << "}";
    timing_out.close();

    return 0;
}
}