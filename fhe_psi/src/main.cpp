#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
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
    cout << "[windowing]" << endl;
    for (auto elem : exponents) {
        cout << elem << " ";
    }
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
    // 각 파티션 별로 다항식 전개한 결과 저장
    vector<Ciphertext> producted = sender_evaluator.intersect(all_powers, coeffs, batch_encoder, evaluator, context);



    return 0;
}