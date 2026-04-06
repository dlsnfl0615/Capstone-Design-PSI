#include <iostream>
#include "seal/seal.h"
#include "windowing.h"

using namespace std;
using namespace seal;

int main() {
    // 1. SEAL 파라미터 설정함
    EncryptionParameters parms(scheme_type::bfv);
    size_t poly_modulus_degree = 8192;
    parms.set_poly_modulus_degree(poly_modulus_degree);
    parms.set_coeff_modulus(CoeffModulus::BFVDefault(poly_modulus_degree));
    parms.set_plain_modulus(PlainModulus::Batching(poly_modulus_degree, 20));

    SEALContext context(parms);

    // 2. 키 생성 및 객체 초기화함
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

    // 3. 테스트 데이터 준비 (y = 5, l = 2, max_degree = 3)
    int y_val = 5;
    int l = 2;
    int max_degree = 3;

    // 다항식: 1*y^3 + 2*y^2 + 3*y + 4
    vector<int64_t> coeff_values = { 4, 3, 2, 1 }; // 상수항부터 시작함
    vector<Plaintext> plain_coeffs;
    for (int64_t c : coeff_values) {
        Plaintext p;
        batch_encoder.encode(vector<int64_t>(batch_encoder.slot_count(), c), p);
        plain_coeffs.push_back(p);
    }

    // 4. 입력값 y 암호화함
    Plaintext plain_y;
    batch_encoder.encode(vector<int64_t>(batch_encoder.slot_count(), y_val), plain_y);
    Ciphertext encrypted_y;
    encryptor.encrypt(plain_y, encrypted_y);

    cout << "--- 테스트 시작 ---" << endl;
    cout << "입력값 y: " << y_val << endl;
    cout << "다항식: y^3 + 2y^2 + 3y + 4" << endl;

    // 5. Receiver: 거듭제곱 조각 생성함
    cout << "Step 1: encrypt_powers 실행 중..." << endl;
    vector<Ciphertext> powers = encrypt_powers(encrypted_y, l, max_degree, encryptor, evaluator, relin_keys);

    // 6. Sender: 다항식 조합 및 연산 실행함
    cout << "Step 2: evaluate_polynomial_combined 실행 중..." << endl;
    Ciphertext result_encrypted = evaluate_polynomial_combined(powers, plain_coeffs, l, max_degree, evaluator, relin_keys);

    // 7. 복호화 및 결과 확인함
    Plaintext plain_result;
    decryptor.decrypt(result_encrypted, plain_result);
    vector<int64_t> result_vec;
    batch_encoder.decode(plain_result, result_vec);

    // 검증: 5^3 + 2(5^2) + 3(5) + 4 = 125 + 50 + 15 + 4 = 194
    int64_t expected = pow(y_val, 3) + 2 * pow(y_val, 2) + 3 * y_val + 4;

    cout << "계산 결과: " << result_vec[0] << endl;
    cout << "기대 결과: " << expected << endl;

    if (result_vec[0] == expected) {
        cout << "결과: 성공!" << endl;
    }
    else {
        cout << "결과: 실패 (노이즈 또는 로직 확인 필요)" << endl;
    }

	return 0;
}