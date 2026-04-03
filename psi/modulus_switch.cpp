#include "modulus_switch.h"
#include <iostream>

// ============================================================
// modulus_switch.cpp
// ------------------------------------------------------------
// SEAL의 modulus switching 관련 helper 구현 파일
//
// 지금 단계에서 할 수 있는 것:
//   1) encrypt한 샘플 ciphertext에 대해 mod switch 테스트
//   2) level이 실제로 내려가는지 출력 확인
//   3) decrypt 결과가 유지되는지 확인
//
// 나중에 sender 담당자가 할 것:
//   - homomorphic evaluation으로 result_cipher 생성
//   - result_cipher에 relinearization / modulus switching 적용
//   - receiver에게 전송
// ============================================================


// ------------------------------------------------------------
// 현재 ciphertext가 어느 level에 있는지 출력
// ------------------------------------------------------------
void print_cipher_level(const Ciphertext& cipher,
    const FHEContext& fhe,
    const string& label) {
    // ciphertext가 현재 어떤 parms_id(= 어떤 level의 파라미터)를 쓰는지
    // SEAL context에서 찾아온다.
    auto context_data = fhe.context->get_context_data(cipher.parms_id());

    if (!context_data) {
        cout << "[" << label << "] invalid parms_id\n";
        return;
    }

    const auto& parms = context_data->parms();
    const auto& coeffs = parms.coeff_modulus();

    cout << "[" << label << "]\n";

    // chain_index:
    //   coeff modulus chain에서 현재 어느 위치인지 알려주는 값
    //   modulus switching을 하면 보통 이 값이 변한다.
    cout << "  chain_index        : " << context_data->chain_index() << "\n";

    // 현재 level에서 사용 중인 coeff_modulus prime 개수
    cout << "  coeff_modulus size : " << coeffs.size() << "\n";

    // 각 prime의 bit 크기를 출력
    // 예: [50, 40, 40] 같은 느낌으로 확인 가능
    cout << "  coeff_modulus bits : [";
    for (size_t i = 0; i < coeffs.size(); i++) {
        cout << coeffs[i].bit_count();
        if (i + 1 != coeffs.size()) cout << ", ";
    }

    // q를 구성하는 소수들 
    cout << "  coeff_modulus vals : [";
    for (size_t i = 0; i < coeffs.size(); i++) {
        cout << coeffs[i].value();
        if (i + 1 != coeffs.size()) cout << ", ";
    }
    cout << "]\n";
}


// ------------------------------------------------------------
// ciphertext 저장 크기(직렬화 크기) 출력
// ------------------------------------------------------------
void print_cipher_size(const Ciphertext& cipher,
    const string& label) {
    // save_size는 "파일로 저장하거나 네트워크로 보낼 때 대략 얼마나 큰지"
    // 확인하는 데 유용하다.
    //
    // compr_mode_type::none:
    //   압축 없이 순수 직렬화 크기 기준
    //
    // modulus switching의 핵심 목적 중 하나가 response ciphertext를
    // 더 작은 modulus level로 내리면서 크기를 줄이는 것이므로,
    // 전/후 비교를 위해 출력한다.
    size_t bytes = cipher.save_size(compr_mode_type::none);

    cout << "[" << label << "] ciphertext size = "
        << bytes << " bytes\n";
}


// ------------------------------------------------------------
// 한 단계 아래 level로 modulus switching
// ------------------------------------------------------------
Ciphertext mod_switch_to_next(const Ciphertext& cipher,
    const FHEContext& fhe) {
    // out에 결과를 저장해서 반환
    Ciphertext out;

    // SEAL evaluator가 현재 ciphertext를
    // "다음 level의 coeff modulus" 위로 옮겨준다.
    //
    // 즉, 우리가 직접 q'를 계산해서 넣는 게 아니라,
    // 현재 parms_id가 가리키는 level의 "next context"로 이동하는 방식이다.
    fhe.evaluator->mod_switch_to_next(cipher, out);

    return out;
}


// ------------------------------------------------------------
// 한 단계 아래 level로 modulus switching (in-place)
// ------------------------------------------------------------
void mod_switch_to_next_inplace(Ciphertext& cipher,
    const FHEContext& fhe) {
    // 원본 ciphertext 자체를 직접 수정
    fhe.evaluator->mod_switch_to_next_inplace(cipher);
}


// ------------------------------------------------------------
// 여러 단계 아래로 modulus switching
// ------------------------------------------------------------
Ciphertext mod_switch_n_times(const Ciphertext& cipher,
    const FHEContext& fhe,
    size_t steps) {
    // 입력 ciphertext를 복사해서 out으로 작업
    Ciphertext out = cipher;

    // steps만큼 반복 시도
    for (size_t i = 0; i < steps; i++) {
        auto context_data = fhe.context->get_context_data(out.parms_id());

        // 현재 level 정보가 없거나,
        // 더 내려갈 next level이 없으면 중단
        if (!context_data || context_data->next_context_data() == nullptr) {
            break;
        }

        // 한 단계 내림
        fhe.evaluator->mod_switch_to_next_inplace(out);
    }

    return out;
}