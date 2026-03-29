#include "seal/seal.h"
#include <vector>
#include <cmath>

using namespace std;
using namespace seal;

/**
 * @brief 수신자 측: Windowing을 위한 암호화된 거듭제곱 생성
 * @param l 윈도우 크기 (bit 단위)
 * @param max_degree 송신자 다항식의 최대 차수 (B)
 */
vector<Ciphertext> encrypt_powers(
    const Ciphertext& encrypted_y,
    int l,
    int max_degree,
    Encryptor& encryptor,
    Evaluator& evaluator,
    RelinKeys& relin_keys) {

    vector<Ciphertext> encrypted_powers;
    int num_j = static_cast<int>(floor(log2(max_degree) / l)) + 1; // j의 범위 계산
    int max_i = (1 << l) - 1; // i의 범위: 1 ~ 2^l - 1

    // 기본 y의 거듭제곱들을 계산하여 저장함
    // 논문 규격: y^{i * 2^{l*j}} 
    for (int j = 0; j < num_j; j++) {
        for (int i = 1; i <= max_i; i++) {
            double exponent = i * pow(2, l * j);
            if (exponent > max_degree) break;

            Ciphertext power;
            if (exponent == 1) {
                power = encrypted_y;
            }
            else {
                // 실제 구현 시 i와 j 조합에 맞춰 효율적인 exponentiation 수행함
                evaluator.exponentiate(encrypted_y, static_cast<uint64_t>(exponent), relin_keys, power);
            }
            encrypted_powers.push_back(move(power));
        }
    }
    return encrypted_powers;
}

/**
 * @brief 송신자 측: Windowing 암호문을 이용한 다항식 점곱 연산
 * @param encrypted_powers 수신자가 보낸 거듭제곱 암호문들
 * @param coefficients 송신자의 세트로 생성된 다항식 계수 (Plaintext 벡터)
 */
Ciphertext evaluate_polynomial_windowing(
    const vector<Ciphertext>& encrypted_powers,
    const vector<Plaintext>& coefficients,
    Evaluator& evaluator) {

    Ciphertext result;
    // 첫 번째 항으로 초기화함 (상수항 제외, 계수와 암호화된 거듭제곱의 점곱)
    // d_i = sum(c_j * y^j) 
    evaluator.multiply_plain(encrypted_powers[0], coefficients[1], result);

    for (size_t k = 1; k < encrypted_powers.size(); k++) {
        if (k + 1 >= coefficients.size()) break;

        Ciphertext temp;
        evaluator.multiply_plain(encrypted_powers[k], coefficients[k + 1], temp);
        evaluator.add_inplace(result, temp); // 결과에 누적함
    }

    // 마지막에 상수항(coefficients[0])을 더해줌
    evaluator.add_plain_inplace(result, coefficients[0]);

    return result;
}