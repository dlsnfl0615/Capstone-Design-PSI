#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <cstdint>
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

int result(const char* storage, const char* csv) {
    cout << "[load] SEAL objects loading..";

    string storage_dir(storage);
    string receiver_csv(csv);

    auto t_load_start = Clock::now();

    // receiver 원본 데이터 다시 로드
    auto receiver_data = load_receiver(receiver_csv);

    // parms 로드
    EncryptionParameters parms;
    ifstream parms_in(storage_dir + "/parms.bin", ios::binary);
    if (!parms_in.is_open()) {
        cerr << "Error: Can't find parms.bin" << endl;
        return 1;
    }
    parms.load(parms_in);
    parms_in.close();

    // request와 같은 context 생성
    SEALContext context(parms);

    // 공개키 로드
    PublicKey public_key;
    ifstream pk_in(storage_dir + "/public_key.bin", ios::binary);
    if (pk_in.is_open()) {
        public_key.load(context, pk_in);
        pk_in.close();
    }

    // relin 키 로드
    RelinKeys relin_keys;
    ifstream rk_in(storage_dir + "/relin_key.bin", ios::binary);
    if (rk_in.is_open()) {
        relin_keys.load(context, rk_in);
        rk_in.close();
    }

    // 비밀키 로드
    SecretKey secret_key;
    ifstream sk_in(storage_dir + "/secret_key.bin", ios::binary);
    if (sk_in.is_open()) {
        secret_key.load(context, sk_in);
        sk_in.close();
    }

    // receiver 해시 테이블 로드
    vector<uint64_t> hash_table;
    ifstream hash_in(storage_dir + "/receiver_hash.bin", ios::binary);
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
    ifstream res_in(storage_dir + "/result.bin", ios::binary);

    if (!res_in.is_open()) {
        cerr << "Error: Can't find result.bin: storage/result.bin" << endl;
        return 1;
    }

    size_t result_size;
    res_in.read(reinterpret_cast<char*>(&result_size), sizeof(size_t));

    cout << "[load] result_size: " << result_size << endl;

    for (size_t i = 0; i < result_size; i++) {
        Ciphertext ct;
        ct.load(context, res_in);
        producted.push_back(move(ct));
    }

    res_in.close();
    auto t_load_end = Clock::now();
    cout << "completed.\n\n";

    auto t_decrypt_start = Clock::now();
    cout << "Final intersection checking..";

    set<uint64_t> intersection_packed_values;           // 디버깅용
    set<pair<int, uint64_t>> intersection_candidates;   // 최종 판정용

    int count = 0;

    for (size_t idx = 0; idx < producted.size(); idx++) {
        int block_idx = static_cast<int>(idx % num_blocks);

        Plaintext plain_result;
        decryptor.decrypt(producted[idx], plain_result);

        vector<uint64_t> decoded_slots;
        batch_encoder.decode(plain_result, decoded_slots);

        for (int slot = 0; slot < n; slot++) {
            int global_bin = block_idx * n + slot;

            if (global_bin >= m) {
                continue;
            }

            if (global_bin >= static_cast<int>(hash_table.size())) {
                continue;
            }

            if (decoded_slots[slot] == 0) {
                uint64_t packed_val = hash_table[global_bin];
                count++;

                if (packed_val != RECEIVER_DUMMY) {
                    intersection_packed_values.insert(packed_val);
                    intersection_candidates.insert({global_bin, packed_val});
                }
            }
        }
    }
    auto t_decrypt_end = Clock::now();
    cout << "completed.\n\n";

    cout << "count: " << count << endl;

    auto t_intersect_start = Clock::now();

    // [receiver result] 원본 데이터와 대조하여 어떤 데이터가 교집합인지
    ofstream intersection_out(storage_dir + "/intersections.csv");
    int found_count = 0;

    ReceiverHashing receiver_hashing;

    cout << "Receiver: Intersection Results is saved in \"intersections.csv\"" << endl;
    for (const auto& record : receiver_data) {
        // 원본 레코드를 다시 패킹하여 비교 대상으로 만듦 (hashing 로직과 동일해야 함)
        string pid_str = record.substr(0, 13);
        string disease_str = record.substr(13, 10);
        uint64_t pid_val = std::stoull(pid_str);
        uint64_t disease_val = std::stoull(disease_str, nullptr, 2);
        uint64_t full_item = (pid_val << 10) | disease_val;

        int shift_bits = get_log2_m();
        uint64_t mask = get_x_r_mask();

        uint64_t x_R = full_item & mask;
        uint64_t x_L = full_item >> shift_bits;

        bool is_intersected = false;

        for (int i = 0; i < h; i++) {
            int loc = static_cast<int>((receiver_hashing.get_hash(x_L, i) % m) ^ x_R);
            uint64_t packed = (x_L << 2) | static_cast<uint64_t>(i);

            if (intersection_candidates.count({loc, packed})) {
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

    double loadMs      = chrono::duration<double, milli>(t_load_end      - t_load_start).count();
    double decryptMs   = chrono::duration<double, milli>(t_decrypt_end   - t_decrypt_start).count();
    double intersectMs = chrono::duration<double, milli>(t_intersect_end - t_intersect_start).count();

    cout << "loadMs: " << loadMs << ", decryptMs: " << decryptMs << ", intersectMs: " << intersectMs << endl;

    ofstream timing_out(storage_dir + "/cpp_timing.json");
    timing_out << fixed << setprecision(3)
               << "{\"loadMs\":"      << loadMs
               << ",\"decryptMs\":"  << decryptMs
               << ",\"intersectMs\":" << intersectMs << "}";
    timing_out.close();

    return 0;
}
}