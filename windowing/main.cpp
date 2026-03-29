#include "seal/seal.h"
#include <vector>
#include <cmath>

using namespace std;
using namespace seal;

/**
 * @brief receiver 측: Windowing을 위한 암호화된 거듭제곱 생성
 * @param encrypted_y receiver가 가지고 있는 데이터 y의 암호문
 * @param l 윈도우 크기 (bit 단위)
 * @param max_degree sender가 가진 다항식의 최대 차수 B
 * @param relinkeys 암호문 간의 곱셈 연산 후에 커진 암호문의 크기를 다시 줄여주는 재선형화 키
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

    // 암호화된 y의 거듭제곱들을 필요한 부분만 계산하여 저장
    for (int j = 0; j < num_j; j++) {
        for (int i = 1; i <= max_i; i++) {
            double exponent = i * pow(2, l * j);
            if (exponent > max_degree) break;

            Ciphertext power;
            if (exponent == 1) {
                power = encrypted_y;
            }
            else {
				// y를 exponent만큼 거듭제곱하지 않고, 암호문끼리의 곱셈이 발생할 때마다 relin_keys 를 사용하여 암호문의 크기를 제어하고 노이즈 증가를 관리
                evaluator.exponentiate(encrypted_y, static_cast<uint64_t>(exponent), relin_keys, power);
            }
            encrypted_powers.push_back(move(power));
        }
    }
    return encrypted_powers;
}

/**
 * @brief sender 측: Windowing 암호문을 이용한 다항식 점곱 연산
 * @param encrypted_powers receiver가 보낸 거듭제곱 암호문들
 * @param coefficients sender의 세트로 생성된 다항식 계수
 */
Ciphertext evaluate_polynomial_windowing(
    const vector<Ciphertext>& encrypted_powers,
    const vector<Plaintext>& coefficients,
    Evaluator& evaluator) {

    Ciphertext result;
    // result를 첫 번째 항(a_1∙y^1)으로 초기화함
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