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
#include "windowing.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

extern "C" {

#ifdef _WIN32
__declspec(dllexport) // 윈도우 환경 DLL 내보내기
#endif

int request(const char* storage, const char* csv) {
    cout << "[setup] Receiver SEAL setting..";

    string storage_dir(storage);
    string receiver_csv(csv);

    EncryptionParameters parms(scheme_type::bfv);
    size_t poly_modulus_degree = n;
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(PlainModulus::Batching(n, t));

    SEALContext context(parms);
    uint64_t plain_modulus = context.first_context_data()->parms().plain_modulus().value();

    // 키 생성 및 객체 초기화
    KeyGenerator keygen(context);
    SecretKey secret_key = keygen.secret_key();
    PublicKey public_key;
    keygen.create_public_key(public_key);
    RelinKeys relin_keys;
    keygen.create_relin_keys(relin_keys);

    Encryptor encryptor(context, public_key);
    Evaluator evaluator(context);
    Decryptor decryptor(context, secret_key);
    BatchEncoder batch_encoder(context);
    cout << "completed\n\n";

    // 데이터 로드
    auto receiver_data = load_receiver(receiver_csv);

    // 해싱
    auto t_hashing_start = Clock::now();
    ReceiverHashing receiver_hashing;
    receiver_hashing.locate(receiver_data);
    auto t_hashing_end = Clock::now();

    // 윈도잉
    auto t_windowing_start = Clock::now();
    Windowing windowing;
    vector<int> exponents = windowing.get_exponents();
    map<int, vector<Ciphertext>> encrypted_powers = windowing.receiver_windowing(
        batch_encoder,
        encryptor,
        plain_modulus,
        exponents,
        receiver_hashing.hash_table);
    auto t_windowing_end = Clock::now();

    // parms 저장
    ofstream parms_out(storage_dir + "/parms.bin", ios::binary);
    parms.save(parms_out);
    parms_out.close();

    // 공개키 저장
    ofstream pk_out(storage_dir + "/public_key.bin", ios::binary);
    public_key.save(pk_out);
    pk_out.close();

    // 비밀키 저장
    ofstream sk_out(storage_dir + "/secret_key.bin", ios::binary);
    secret_key.save(sk_out);
    sk_out.close();

    // relin 키 저장
    ofstream rk_out(storage_dir + "/relin_key.bin", ios::binary);
    relin_keys.save(rk_out);
    rk_out.close();

    // 해시 테이블 저장
    ofstream hash_out(storage_dir + "/receiver_hash.bin", ios::binary);
    size_t table_size = receiver_hashing.hash_table.size();
    hash_out.write(reinterpret_cast<const char*>(&table_size), sizeof(size_t));
    hash_out.write(reinterpret_cast<const char*>(receiver_hashing.hash_table.data()), table_size * sizeof(uint64_t));
    hash_out.close();

    // 윈도잉 결과 저장 - receiver 쪽
    ofstream ofs(storage_dir + "/powers.bin", ios::binary);

    if (!ofs.is_open()) {
        cerr << "Error: Can't open storage/powers.bin" << endl;
        return 1;
    }

    size_t map_size = encrypted_powers.size();
    size_t block_size = num_blocks;

    ofs.write(reinterpret_cast<const char*>(&map_size), sizeof(size_t));
    ofs.write(reinterpret_cast<const char*>(&block_size), sizeof(size_t));

    for (auto& kv : encrypted_powers) {
        int key = kv.first;
        ofs.write(reinterpret_cast<const char*>(&key), sizeof(int));

        for (int b = 0; b < num_blocks; b++) {
            kv.second[b].save(ofs);
        }
    }

    ofs.close();

    double hashingMs = chrono::duration<double, milli>(t_hashing_end - t_hashing_start).count();
    double windowingMs = chrono::duration<double, milli>(t_windowing_end - t_windowing_start).count();

    cout << "hashingMs: " << hashingMs << ", windowingMs: " << windowingMs << endl;

    ofstream timing_out(storage_dir + "/cpp_timing.json");
    timing_out << fixed << setprecision(3)
         << "{\"hashingMs\":"    << hashingMs
         << ",\"windowingMs\":" << windowingMs << "}";
    timing_out.close();

    cout << "Receiver Request completed." << endl;

    return 0;
}
}