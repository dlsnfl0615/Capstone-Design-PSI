#pragma once

#include "fhe_setup.h"
#include <string>
using namespace std;
using namespace seal;

// ============================================================
// modulus_switch.h
// ------------------------------------------------------------
// 목적:
//   - SEAL의 modulus switching 기능을 공통 helper 형태로 제공
//   - ciphertext가 현재 어느 level(parms_id)에 있는지 확인
//   - 한 단계 / 여러 단계 modulus switching 수행
//   - 나중에 sender 담당자가 만든 "결과 ciphertext"에 바로 적용 가능
//
//   - 지금 단계에서는 "실제 PSI 결과 ciphertext"가 아직 없으므로
//     샘플 ciphertext로 기능 테스트를 먼저 할 수 있다.
//   - 나중에는 sender가 homomorphic evaluation 결과를 만든 뒤,
//     receiver에게 돌려주기 직전에 이 helper를 호출하면 된다.
// ============================================================


// ------------------------------------------------------------
// 현재 ciphertext가 어느 level에 있는지 출력
//
// 출력 내용 예시:
//   - chain_index: 현재 coeff modulus chain에서의 위치
//   - coeff_modulus 개수
//   - 각 coeff modulus prime의 bit 크기
//
// 왜 필요한가?
//   - modulus switching 전/후에 ciphertext가 실제로
//     "다음 level"로 내려갔는지 확인하기 위해 사용
// ------------------------------------------------------------
void print_cipher_level(const seal::Ciphertext& cipher,
    const FHEContext& fhe,
    const std::string& label = "cipher");


// ------------------------------------------------------------
// ciphertext 저장 크기(대략적인 직렬화 크기)를 출력
//
// 왜 필요한가?
//   - modulus switching의 목적 중 하나가
//     "response ciphertext 크기 감소"이므로,
//     전/후 크기를 비교하기 위해 사용
// ------------------------------------------------------------
void print_cipher_size(const seal::Ciphertext& cipher,
    const std::string& label = "cipher");


// ------------------------------------------------------------
// 한 단계 아래 level로 modulus switching
//
// 입력:
//   - cipher : 현재 ciphertext
//   - fhe    : FHEContext (Evaluator / Context 사용)
//
// 반환:
//   - 한 단계 아래 level로 이동한 새로운 ciphertext
//
// 특징:
//   - 원본 cipher는 그대로 유지
//   - 복사본을 만들어 next level로 이동시킨 뒤 반환
// ------------------------------------------------------------
seal::Ciphertext mod_switch_to_next(const seal::Ciphertext& cipher,
    const FHEContext& fhe);


// ------------------------------------------------------------
// 한 단계 아래 level로 modulus switching (in-place)
//
// 입력:
//   - cipher : 현재 ciphertext (직접 수정됨)
//   - fhe    : FHEContext
//
// 특징:
//   - 원본 ciphertext 자체를 바로 변경
//   - sender 쪽에서 결과 ciphertext를 바로 줄이고 싶을 때 유용
// ------------------------------------------------------------
void mod_switch_to_next_inplace(seal::Ciphertext& cipher,
    const FHEContext& fhe);


// ------------------------------------------------------------
// 여러 단계 아래로 modulus switching
//
// 입력:
//   - cipher : 현재 ciphertext
//   - fhe    : FHEContext
//   - steps  : 최대 몇 단계 내릴지
//
// 반환:
//   - 가능한 만큼 modulus switching이 적용된 새로운 ciphertext
//
// 동작:
//   - next level이 남아 있으면 한 단계씩 내림
//   - 더 이상 next level이 없으면 중단
//
// 주의:
//   - "몇 단계까지 내리는 게 안전한가?"는
//     실제 sender 연산 결과와 noise budget을 보고 정해야 함
//   - 지금 단계에서는 기능 테스트용으로 사용
// ------------------------------------------------------------
seal::Ciphertext mod_switch_n_times(const seal::Ciphertext& cipher,
    const FHEContext& fhe,
    size_t steps);