#include <iostream>
#include "data_loader.h"
#include "hashing.h"
#include "psi_receiver.h"
#include "fhe_setup.h"
#include "../parameters.h"

using namespace std;

int main() {
    try {
        // 1. 데이터 로드
        cout << "[Step 1] Loading receiver data..." << endl;
        auto receiver_data = load_receiver("data/receiver.csv");

        // 2. 뻐꾸기 해싱 및 순열 기반 해싱 수행
        cout << "[Step 2] Performing Cuckoo Hashing..." << endl;
        Hashing hashing;
        hashing.locate(receiver_data);

        // 3. Microsoft SEAL 환경 설정 (FHESetup 클래스 활용)
        cout << "[Step 3] Setting up Microsoft SEAL..." << endl;
        FHESetup fhe;
        fhe.setup(n, t); // parameters.h의 n 사용

        // 4. 배칭 (SIMD 슬롯 배치)
        cout << "[Step 4] Batching hash table into slots..." << endl;
        // fhe.batch_encoder를 통해 슬롯 수에 접근
        vector<uint64_t> batched_vec = hashing.batching(fhe.batch_encoder->slot_count());

        // 5. 암호화
        cout << "[Step 5] Encrypting batched data..." << endl;
        seal::Plaintext plain_y;
        fhe.batch_encoder->encode(batched_vec, plain_y); // fhe 객체의 encoder 사용
        seal::Ciphertext encrypted_y;
        fhe.encryptor->encrypt(plain_y, encrypted_y);    // fhe 객체의 encryptor 사용

        // 6. 윈도잉 기법 적용
        cout << "[Step 6] Generating windowed powers..." << endl;
        PsiReceiver psi_receiver;
        auto windowed_powers = psi_receiver.generate_windowed_powers(
            *fhe.context, // context 인자 추가
            encrypted_y, 
            l, 
            B, 
            *fhe.evaluator, 
            fhe.relin_keys);

        cout << "\n[Success] Receiver side testing complete." << endl;
        cout << "Generated " << windowed_powers.size() << " windowed powers." << endl;

        psi_receiver.save_powers(windowed_powers, "powers.bin");
        fhe.public_key.save(ofstream("public_key.bin", ios::binary));
        fhe.relin_keys.save(ofstream("relin_keys.bin", ios::binary));

        // 2. 나중에 내가(Receiver) 쓸 파일 저장 (결과 확인용)
        fhe.secret_key.save(ofstream("secret_key.bin", ios::binary));

        // 3. 해시 테이블 저장 (결과 복구용)
        ofstream ht_file("hash_table.bin", ios::binary);
        for(uint64_t val : hashing.hash_table) {
            ht_file.write(reinterpret_cast<const char*>(&val), sizeof(uint64_t));
        }

    } catch (const std::exception& e) {
        cerr << "\n[Error] Exception occurred: " << e.what() << endl;
        return 1;
    }
    return 0;
}