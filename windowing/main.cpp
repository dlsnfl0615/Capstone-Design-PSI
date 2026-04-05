#include "seal/seal.h"
#include <vector>
#include <cmath>

using namespace std;
using namespace seal;

/** * @brief receiver 측: Windowing을 위한 암호화된 거듭제곱 생성
 * @param encrypted_y receiver가 가지고 있는 데이터 y의 암호문 [cite: 335]
 * @param l 윈도우 크기 (bit 단위) [cite: 337, 470]
 * @param max_degree sender가 가진 다항식의 최대 차수 B [cite: 136, 470]
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
    int num_j = static_cast<int>(floor(log2(max_degree) / l)) + 1; // j의 범위 계산 [cite: 337]
    int max_i = (1 << l) - 1; // i의 범위: 1 ~ 2^l - 1 [cite: 337]

    // 암호화된 y의 거듭제곱들을 필요한 부분만 계산하여 저장 [cite: 478]
    for (int j = 0; j < num_j; j++) {
        for (int i = 1; i <= max_i; i++) {
            double exponent = i * pow(2, l * j); // 지수 공식: i * 2^{lj} [cite: 337]
            if (exponent > max_degree) break;

            Ciphertext power;
            if (exponent == 1) {
                power = encrypted_y;
            }
            else {
                // y를 exponent만큼 거듭제곱하지 않고, 암호문끼리의 곱셈이 발생할 때마다 
                // relin_keys 를 사용하여 암호문의 크기를 제어하고 노이즈 증가를 관리 [cite: 481]
                evaluator.exponentiate(encrypted_y, static_cast<uint64_t>(exponent), relin_keys, power);
            }
            encrypted_powers.push_back(move(power)); // 생성된 조각 저장 [cite: 479]
        }
    }
    return encrypted_powers;
}

/**
 * @brief sender 측: Windowing 조각들을 조합하여 다항식 연산 수행함
 */
Ciphertext evaluate_polynomial_combined(
    const vector<Ciphertext>& windowed_powers,
    const vector<Plaintext>& coefficients,
    int l,
    int max_degree,
    Evaluator& evaluator,
    RelinKeys& relin_keys) {

    // 모든 필요한 지수(y^1 ~ y^B)를 담을 테이블임
    vector<Ciphertext> all_powers(max_degree + 1);
    int max_i = (1 << l) - 1;

    // 1. 수신자가 보낸 기초 조각들을 지수 위치에 매핑함
    int idx = 0;
    int num_j = static_cast<int>(floor(log2(max_degree) / l)) + 1;
    for (int j = 0; j < num_j; j++) {
        for (int i = 1; i <= max_i; i++) {
            int exp_val = i * (1 << (l * j)); // 기초 조각의 실제 지수 계산함
            if (exp_val > max_degree) break;
            all_powers[exp_val] = windowed_powers[idx++]; // 테이블에 배치함
        }
    }

    // 2. 비어있는 지수 k를 2^l 진법으로 분해하여 조합함
    for (int k = 1; k <= max_degree; k++) {
        if (!all_powers[k].is_transparent()) continue; // 조각이 이미 있으면 스킵함

        Ciphertext combined;
        bool first = true;
        int temp_k = k;
        int j = 0;
        int base = (1 << l);

        while (temp_k > 0) {
            int i = temp_k % base; // 현재 자리수의 값 d_j 도출함
            if (i > 0) {
                int part_exp = i * (1 << (l * j)); // 해당 항의 지수임
                if (first) {
                    combined = all_powers[part_exp];
                    first = false;
                }
                else {
                    evaluator.multiply_inplace(combined, all_powers[part_exp]); // 암호문끼리 곱함
                    evaluator.relinearize_inplace(combined, relin_keys); // 크기 관리함
                }
            }
            temp_k /= base;
            j++;
        }
        all_powers[k] = combined; // 조합된 결과 저장함
    }

    // 3. 완성된 지수들로 다항식 점곱 계산함
    Ciphertext result;
    evaluator.multiply_plain(all_powers[1], coefficients[1], result); // 1차항 초기화함
    for (int k = 2; k <= max_degree; k++) {
        Ciphertext temp;
        evaluator.multiply_plain(all_powers[k], coefficients[k], temp); // 평문 계수와 곱함
        evaluator.add_inplace(result, temp); // 합산함
    }
    evaluator.add_plain_inplace(result, coefficients[0]); // 상수항 추가함

    return result;
}