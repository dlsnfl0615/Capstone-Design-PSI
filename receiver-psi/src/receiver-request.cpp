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
    try {
        cout << "[setup] Receiver SEAL setting.." << endl;

        // 데이터 로드
        auto receiver_data = load_receiver("data/B_receiver_5300.csv");

        // 객체 선언
        EncryptionParameters parms(scheme_type::bfv);
        parms.set_poly_modulus_degree(n);
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
        
        // 해싱
        ReceiverHashing receiver_hashing;
        receiver_hashing.locate(receiver_hashing.compress(receiver_data));

        // 윈도잉
        Windowing windowing;
        vector<int> exponents = windowing.get_exponents();
        map<int, vector<Ciphertext>> encrypted_powers = windowing.receiver_windowing(
            batch_encoder,
            encryptor,
            plain_modulus,
            exponents,
            receiver_hashing.hash_table);

        // parms 저장
        ofstream parms_out("../data/parms.bin", ios::binary);
        parms.save(parms_out);
        parms_out.close();
        
        ofstream parms_out_1("C:/psi_0515/Capstone-Design-PSI-sender/sender-psi/data/parms.bin", ios::binary);
        parms.save(parms_out_1);
        parms_out_1.close();

        // 공개키 저장
        ofstream pk_out("../data/public_key.bin", ios::binary);
        public_key.save(pk_out);
        pk_out.close();

        ofstream pk_out_1("C:/psi_0515/Capstone-Design-PSI-sender/sender-psi/data/public_key.bin", ios::binary);
        public_key.save(pk_out_1);
        pk_out_1.close();

        // 비밀키 저장
        ofstream sk_out("../data/secret_key.bin", ios::binary);
        secret_key.save(sk_out);
        sk_out.close();

        // relin 키 저장
        ofstream rk_out("../data/relin_key.bin", ios::binary);
        relin_keys.save(rk_out);
        rk_out.close();

        ofstream rk_out_1("C:/psi_0515/Capstone-Design-PSI-sender/sender-psi/data/relin_key.bin", ios::binary);
        relin_keys.save(rk_out_1);
        rk_out_1.close();

        // 해시 테이블 저장
        ofstream hash_out("../data/receiver_hash.bin", ios::binary);
        size_t table_size = receiver_hashing.hash_table.size();
        hash_out.write(reinterpret_cast<const char*>(&table_size), sizeof(size_t));
        hash_out.write(reinterpret_cast<const char*>(receiver_hashing.hash_table.data()), table_size * sizeof(uint64_t));
        hash_out.close();

        // 윈도잉 결과 저장
        // 변경 이유:
        // 기존 powers.bin 구조:
        //   map_size
        //   key, ciphertext
        //   key, ciphertext
        //   ...
        //
        // 변경 powers.bin 구조:
        //   map_size
        //   block_size
        //   key, ciphertext_block_0, ciphertext_block_1, ...
        //   key, ciphertext_block_0, ciphertext_block_1, ...
        //
        // sender가 읽을 때 exponent별로 몇 개의 block ciphertext가 있는지 알아야 하므로
        // block_size도 함께 저장함.
        ofstream ofs("../data/powers.bin", ios::binary);

        if (!ofs.is_open()) {
            cerr << "Error: ../data/powers.bin 저장 경로를 열 수 없습니다." << endl;
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

        // sender가 실제로 읽는 powers.bin 경로로 복사 저장
        // 지금 사용하시는 sender data 폴더 기준 경로입니다.
        ofstream ofs_1("C:/psi_0515/Capstone-Design-PSI-sender/sender-psi/data/powers.bin", ios::binary);

        if (!ofs_1.is_open()) {
            cerr << "Error: sender 쪽 data/powers.bin 저장 경로를 열 수 없습니다." << endl;
            return 1;
        }

        ofs_1.write(reinterpret_cast<const char*>(&map_size), sizeof(size_t));
        ofs_1.write(reinterpret_cast<const char*>(&block_size), sizeof(size_t));

        for (auto& kv : encrypted_powers) {
            int key = kv.first;
            ofs_1.write(reinterpret_cast<const char*>(&key), sizeof(int));

            for (int b = 0; b < num_blocks; b++) {
                kv.second[b].save(ofs_1);
            }
        }

        ofs_1.close();

        cout << "Receiver Request completed." << endl;

        return 0;
    } catch (exception e) {
        cerr << e.what() << endl;
    }
}