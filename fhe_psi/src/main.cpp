#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include "parameters.h"
#include "data_loader.h"
#include "receiver_hashing.h"
#include "sender_hashing.h"
#include "windowing.h"
#include "evaluate.h"

using namespace std;
using namespace seal;

int main() {
    // 1. 파라미터 설정: BFV 스킴 사용
    EncryptionParameters parms(scheme_type::bfv);
    size_t poly_modulus_degree = n;
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    // 배칭을 위해 PlainModulus::Batching 사용
    parms.set_plain_modulus(PlainModulus::Batching(n, t));

    SEALContext context(parms);
    uint64_t plain_modulus = context.first_context_data()->parms().plain_modulus().value();
    cout << "plain_modulus: " << plain_modulus << endl;

    // 2. 키 생성 및 객체 초기화
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

    auto receiver_data = load_receiver("data/receiver.csv");
    auto sender_data = load_sender("data/sender.csv");
    
    ReceiverHashing receiver_hashing;
    receiver_hashing.locate(receiver_data);

    Windowing windowing;
    vector<int> exponents = windowing.get_exponents();
    cout << endl;
    map<int, Ciphertext> encrypted_powers = windowing.receiver_windowing(
        batch_encoder,
        encryptor,
        plain_modulus,
        exponents,
        receiver_hashing.hash_table);
        

    SenderHashing sender_hashing;
    sender_hashing.locate(sender_data);

    SenderEvaluate sender_evaluator;
    auto partitioned = sender_evaluator.partitioning(sender_hashing.hash_table);
    auto coeffs = sender_evaluator.extract_all_coefficients(partitioned, plain_modulus);
    
    map<int, Ciphertext> all_powers = sender_evaluator.make_all_powers(encrypted_powers, evaluator, relin_keys);

    vector<uint64_t> constant_one(n, 1);
    Plaintext plain_one;
    batch_encoder.encode(constant_one, plain_one);
    Ciphertext encrypted_one;
    encryptor.encrypt(plain_one, encrypted_one);
    all_powers[0] = encrypted_one;
    

    // 각 파티션 별로 다항식 전개한 결과 저장
    vector<Ciphertext> producted = sender_evaluator.product(all_powers, coeffs, batch_encoder, evaluator, context);
    
    cout << "\n--- Receiver: Final Intersection Check ---" << endl;

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

    // 3. 원본 데이터를 순회하며 교집합 여부를 대조하여 출력함
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

    cout << "\nTotal Intersections: " << found_count << " / " << receiver_data.size() << endl;

    return 0;
}