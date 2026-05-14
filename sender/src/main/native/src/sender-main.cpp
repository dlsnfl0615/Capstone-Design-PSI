#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <chrono>
#include <iomanip>
#include <fstream>
#include "parameters.h"
#include "data_loader.h"
#include "hashing.h"
#include "evaluate.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

extern "C" {
#ifdef _WIN32
__declspec(dllexport) // 윈도우 환경 DLL 내보내기
#endif
int intersect() {
    cout << "[load]SEAL objects loading..";
    // receiver가 보내준 값 가져오기

    auto t_load_start = Clock::now();

    EncryptionParameters parms;
    ifstream parms_in("storage/parms.bin", ios::binary);
    if (!parms_in.is_open()) {
        cerr << "Error: parms.bin 파일을 찾을 수 없습니다." << endl;
        return 1;
    }
    parms.load(parms_in);
    parms_in.close();

    SEALContext context(parms);

    PublicKey public_key;
    ifstream pk_in("storage/public_key.bin", ios::binary);
    if (pk_in.is_open()) {
        public_key.load(context, pk_in);
        pk_in.close();
    }

    RelinKeys relin_keys;
    ifstream rk_in("storage/relin_key.bin", ios::binary);
    if (rk_in.is_open()) {
        relin_keys.load(context, rk_in);
        rk_in.close();
    }

    map<int, Ciphertext> encrypted_powers;
    ifstream ofs_in("storage/powers.bin", ios::binary);
    if (ofs_in.is_open()) {
        size_t map_size;
        // 맵의 크기 먼저 읽기
        ofs_in.read(reinterpret_cast<char*>(&map_size), sizeof(size_t));

        for (size_t i = 0; i < map_size; i++) {
            int key;
            // key (int) 읽기
            ofs_in.read(reinterpret_cast<char*>(&key), sizeof(int));

            // value (Ciphertext) 읽기
            Ciphertext ct;
            ct.load(context, ofs_in);
            
            encrypted_powers[key] = move(ct);
        }
        ofs_in.close();
    }

    Evaluator evaluator(context);
    BatchEncoder batch_encoder(context);
    Encryptor encryptor(context, public_key);
    uint64_t plain_modulus = context.first_context_data()->parms().plain_modulus().value();

    auto sender_data = load_sender("storage/sender.csv");

    auto t_load_end = Clock::now();
    cout << "completed" << "\n\n";

    auto t_hashing_start = Clock::now();
    // 해싱
    SenderHashing sender_hashing;
    sender_hashing.locate(sender_data);
    auto t_hashing_end = Clock::now();

    // 파티셔닝 및 계수 연산
    cout << "[partitioning]\n";
    auto t_partitioning_start = Clock::now();
    SenderEvaluate sender_evaluator;
    auto partitioned = sender_evaluator.partitioning(sender_hashing.hash_table);
    auto coeffs = sender_evaluator.extract_all_coefficients(partitioned, plain_modulus);
    map<int, Ciphertext> all_powers = sender_evaluator.make_all_powers(encrypted_powers, evaluator, relin_keys);

    // 0승 따로 계산
    vector<uint64_t> constant_one(n, 1);
    Plaintext plain_one;
    batch_encoder.encode(constant_one, plain_one);
    Ciphertext encrypted_one;
    encryptor.encrypt(plain_one, encrypted_one);
    all_powers[0] = encrypted_one;
    auto t_partitioning_end = Clock::now();
    
    // 다항식 연산
    auto t_product_start = Clock::now();
    vector<Ciphertext> producted = sender_evaluator.product(all_powers, coeffs, batch_encoder, evaluator, context);
    auto t_product_end = Clock::now();

    // 모듈러스 스위칭
    auto t_modulus_switching_start = Clock::now();
    for (Ciphertext& polynomial : producted) {
        evaluator.mod_switch_to_inplace(polynomial, context.last_parms_id());
    }
    auto t_modulus_switching_end = Clock::now();
    
    // 다항식 연산 결과 저장
    ofstream result_out("storage/result.bin", ios::binary);
    size_t result_size = producted.size();
    result_out.write(reinterpret_cast<const char*>(&result_size), sizeof(size_t));
    for (const auto& ct : producted) {
        ct.save(result_out); //
    }
    result_out.close();

    /* double loadMs = chrono::duration<double, milli>(t_load_end - t_load_start).count();
    double decryptMs = chrono::duration<double, milli>(t_decrypt_end - t_decrypt_start).count();
    double intersectMs = chrono::duration<double, milli>(t_intersect_end - t_intersect_start).count();

    ofstream timing_out("storage/cpp_timing.json");
    timing_out << fixed << setprecision(3)
               << "{\"loadMs\":" << loadMs
               << ",\"decryptMs\":" << decryptMs
               << ",\"intersectMs\":" << intersectMs << "}";
    timing_out.close(); */

    // C++ 단계별 소요 시간 저장
    double loadMs = chrono::duration<double, milli>(t_load_end - t_load_start).count();
    double hashingMs = chrono::duration<double, milli>(t_hashing_end - t_hashing_start).count();
    double partitioningMs = chrono::duration<double, milli>(t_partitioning_end - t_partitioning_start).count();
    double productMs = chrono::duration<double, milli>(t_product_end - t_product_start).count();
    double modulusMs = chrono::duration<double, milli>(t_modulus_switching_end - t_modulus_switching_start).count();

    ofstream timing_out("storage/cpp_timing.json");
    timing_out << fixed << setprecision(3)
               << "{\"loadMs\":" << loadMs
               << ",\"hashingMs\":" << hashingMs
               << ",\"partitioningMs\":" << partitioningMs
               << ",\"productMs\":" << productMs
               << ",\"modulusMs\":" << modulusMs << "}";
    timing_out.close();

    cout << "Result generated." << endl;

    return 0;
}
}