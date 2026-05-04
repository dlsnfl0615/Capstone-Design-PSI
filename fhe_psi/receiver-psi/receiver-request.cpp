#include "seal/seal.h"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include "parameters.h"
#include "data_loader.h"
#include "receiver_hashing.h"
#include "sender_hashing.h"
#include "windowing.h"
#include "evaluate.h"

using namespace std;
using namespace seal;
using Clock = chrono::high_resolution_clock;
using Ms = chrono::milliseconds;

int main() {
    auto total_start = Clock::now();
    // [receiver request] 파라미터 설정
    EncryptionParameters parms(scheme_type::bfv);
    size_t poly_modulus_degree = n;
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(PlainModulus::Batching(n, t));

    SEALContext context(parms);
    uint64_t plain_modulus = context.first_context_data()->parms().plain_modulus().value();
    cout << "plain_modulus: " << plain_modulus << endl;

    // [receiver request] 키 생성 및 객체 초기화
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

    // [receiver request] 데이터 로드
    auto receiver_data = load_receiver("data/receiver.csv");
    auto sender_data = load_sender("data/sender.csv");
    
    // [receiver request] 해싱
    ReceiverHashing receiver_hashing;
    receiver_hashing.locate(receiver_data);

    // [receiver request] 윈도잉
    Windowing windowing;
    vector<int> exponents = windowing.get_exponents();
    cout << endl;
    map<int, Ciphertext> encrypted_powers = windowing.receiver_windowing(
        batch_encoder,
        encryptor,
        plain_modulus,
        exponents,
        receiver_hashing.hash_table);
        
    // [receiver request] sender로 EncryptionParameters 객체, 윈도잉 값, 키 보내기

    
    return 0;
}