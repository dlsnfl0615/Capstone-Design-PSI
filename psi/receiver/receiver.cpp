#include <iostream>
#include "data_loader.h"
#include "hashing.h"
#include "psi_receiver.h"
#include "../parameters.h"
#include "seal/seal.h"

using namespace std;

int main() {
    try {
        // 1. 데이터 로드 (CSV 파일 경로 확인 필요)
        cout << "[Step 1] Loading receiver data..." << endl;
        auto receiver_data = load_receiver("data/receiver.csv");

        // 2. 뻐꾸기 해싱 및 순열 기반 해싱 수행
        cout << "[Step 2] Performing Cuckoo Hashing..." << endl;
        Hashing hashing;
        hashing.locate(receiver_data);
        // hashing.print_hash_table();

        // 3. Microsoft SEAL 환경 설정
        cout << "[Step 3] Setting up Microsoft SEAL..." << endl;
        seal::EncryptionParameters parms(seal::scheme_type::bfv);
        size_t poly_modulus_degree = n; // n
        parms.set_poly_modulus_degree(poly_modulus_degree);
        parms.set_coeff_modulus(seal::CoeffModulus::BFVDefault(poly_modulus_degree));
        // t 값은 패킹된 43비트 데이터보다 커야 함 (예: 44비트 이상의 소수)
        parms.set_plain_modulus(seal::PlainModulus::Batching(poly_modulus_degree, 44));

        seal::SEALContext context(parms);
        seal::KeyGenerator keygen(context);
        seal::SecretKey secret_key = keygen.secret_key();
        seal::PublicKey public_key;
        keygen.create_public_key(public_key);
        seal::RelinKeys relin_keys;
        keygen.create_relin_keys(relin_keys);

        seal::Encryptor encryptor(context, public_key);
        seal::Evaluator evaluator(context);
        seal::BatchEncoder batch_encoder(context);

        // 4. 배칭 (SIMD 슬롯 배치)
        cout << "[Step 4] Batching hash table into slots..." << endl;
        vector<uint64_t> batched_vec = hashing.batching(batch_encoder.slot_count());

        // 5. 암호화
        cout << "[Step 5] Encrypting batched data..." << endl;
        seal::Plaintext plain_y;
        batch_encoder.encode(batched_vec, plain_y);
        seal::Ciphertext encrypted_y;
        encryptor.encrypt(plain_y, encrypted_y);

        // 6. 윈도잉 기법 적용 (거듭제곱 조각 생성)
        cout << "[Step 6] Generating windowed powers..." << endl;
        PsiReceiver psi_receiver;
        int l = 3;      // 윈도우 크기
        int B = 74;     // 최대 차수
        auto windowed_powers = psi_receiver.generate_windowed_powers(
            encrypted_y, l, B, evaluator, relin_keys);

        cout << "\n[Success] Receiver side testing complete." << endl;
        cout << "Generated " << windowed_powers.size() << " windowed powers." << endl;

        // 7. 복구 로직 테스트 (첫 번째 유효 슬롯 기준)
        for (int i = 0; i < hashing.hash_table.size(); i++) {
            if (hashing.hash_table[i] != 0) {
                cout << "\n[Test] Restoring data at loc " << i << "..." << endl;
                RestoredData restored = hashing.restore_original_data(hashing.hash_table[i], i);
                cout << "Restored PID: " << restored.pid << ", Disease: " << restored.disease << endl;
                break; 
            }
        }

    } catch (const std::exception& e) {
        cerr << "\n[Error] Exception occurred: " << e.what() << endl;
        return 1;
    }

    return 0;
}