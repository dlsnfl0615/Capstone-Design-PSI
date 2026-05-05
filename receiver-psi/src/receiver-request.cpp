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
#include "windowing.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

int main() {
    // 파라미터 설정
    EncryptionParameters parms(scheme_type::bfv);
    size_t poly_modulus_degree = n;
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(PlainModulus::Batching(n, t));

    SEALContext context(parms);
    uint64_t plain_modulus = context.first_context_data()->parms().plain_modulus().value();
    cout << "plain_modulus: " << plain_modulus << endl;

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

    // 데이터 로드
    auto receiver_data = load_receiver("data/receiver.csv");
    
    // 해싱
    ReceiverHashing receiver_hashing;
    receiver_hashing.locate(receiver_data);

    // 윈도잉
    Windowing windowing;
    vector<int> exponents = windowing.get_exponents();
    cout << endl;
    map<int, Ciphertext> encrypted_powers = windowing.receiver_windowing(
        batch_encoder,
        encryptor,
        plain_modulus,
        exponents,
        receiver_hashing.hash_table);
        
    // parms 저장
    ofstream parms_out("../data/parms.bin", ios::binary);
    parms.save(parms_out);
    parms_out.close();

    // 공개키 저장
    ofstream pk_out("../data/public_key.bin", ios::binary);
    public_key.save(pk_out);
    pk_out.close();

    // 비밀키 저장
    ofstream sk_out("../data/secret_key.bin", ios::binary);
    secret_key.save(sk_out);
    sk_out.close();

    // relin 키 저장
    ofstream rk_out("../data/relin_key.bin", ios::binary);
    relin_keys.save(rk_out);
    rk_out.close();

    // 해시 테이블 저장
    ofstream hash_out("../data/receiver_hash.bin", ios::binary);
    size_t table_size = receiver_hashing.hash_table.size();
    hash_out.write(reinterpret_cast<const char*>(&table_size), sizeof(size_t));
    hash_out.write(reinterpret_cast<const char*>(receiver_hashing.hash_table.data()), table_size * sizeof(uint64_t));
    hash_out.close();

    // 윈도잉 결과 저장
    ofstream ofs("../data/powers.bin", ios::binary);
    size_t map_size = encrypted_powers.size();
    ofs.write(reinterpret_cast<const char*>(&map_size), sizeof(size_t));
    for (auto &kv : encrypted_powers) {
        // key (int) 저장
        int key = kv.first;
        ofs.write(reinterpret_cast<const char*>(&key), sizeof(int));
        
        // value (Ciphertext) 저장
        kv.second.save(ofs); 
    }
    ofs.close();

    return 0;
}