#pragma once

#include "seal/seal.h"
#include <vector>
#include <cstdint>
#include <memory>
using namespace std;
using namespace seal;

// FHE 파라미터를 묶어서 들고 다니는 구조체
// 하드코딩 대신 이걸 함수 인자로 넘기는 방식
struct FHEParams {
    size_t   poly_modulus_degree;  // n: 다항식 차수 (8192, 16384 등)
    uint64_t  plain_modulus_bit_size;        // t: 평문 모듈러스 (배칭 가능한 소수여야 함)
    int      sec_level;            // 보안 수준 (128, 192, 256)
};

// FHE 컨텍스트 + 키들을 묶어서 들고 다니는 구조체
struct FHEContext {
    std::shared_ptr<seal::SEALContext>    context;
    seal::SecretKey                       secret_key;
    seal::PublicKey                       public_key;
    seal::RelinKeys                       relin_keys;
    std::shared_ptr<seal::BatchEncoder>   encoder;
    std::shared_ptr<seal::Encryptor>      encryptor;
    std::shared_ptr<seal::Decryptor>      decryptor;
    std::shared_ptr<seal::Evaluator>      evaluator;
    size_t                                slot_count; // 배칭 슬롯 수 = n/2
};

// context 생성
// params를 받아서 SEALContext를 만들고 유효성 검사까지
seal::SEALContext create_context(const FHEParams& params);

// 키 생성 (public, secret, relin)
// context를 받아서 키 세트 생성
void generate_keys(FHEContext& fhe);

// FHEContext 전체 초기화 (create_context + generate_keys 한번에)
FHEContext setup_fhe(const FHEParams& params);

// 벡터 → 평문 다항식 (배칭)
// input: uint64_t 벡터 (해싱 담당자가 넘겨주는 형태)
seal::Plaintext encode_batch(const std::vector<uint64_t>& input,
    const FHEContext& fhe);

// 평문 → 암호문
seal::Ciphertext encrypt_batch(const seal::Plaintext& plain,
    const FHEContext& fhe);

// 암호문 → 평문
seal::Plaintext decrypt_batch(const seal::Ciphertext& cipher,
    const FHEContext& fhe);

// 암호문 → uint64_t 벡터 (decode까지 한번에)
std::vector<uint64_t> decrypt_decode(const seal::Ciphertext& cipher,
    const FHEContext& fhe);

// 파라미터 정보 출력 (디버깅용)
void print_parameters(const FHEContext& fhe);

// round-trip 테스트 (encode → encrypt → decrypt → decode 후 원본과 비교)
bool test_roundtrip(const FHEContext& fhe);

// 입력 벡터를 받아 round-trip 검증
// raw_input을 slot_count 크기에 맞게 padding한 뒤
// encode -> encrypt -> decrypt -> decode 결과가 같은지 확인
bool test_roundtrip_with_input(const std::vector<uint64_t>& raw_input,
    const FHEContext& fhe,
    const std::string& label = "FHE");

bool test_roundtrip_all_inputs(const std::vector<uint64_t>& raw_input,
    const FHEContext& fhe,
    const std::string& label = "FHE");


//03.31 : 과정을 보여주는 디버그 함수 추가
// 디버그용: 벡터 앞부분 출력
void print_vector_preview(const std::vector<uint64_t>& data,
    const std::string& label,
    size_t max_items = 8);

// 디버그용: plaintext를 decode해서 앞부분 출력
void print_plain_preview(const seal::Plaintext& plain,
    const FHEContext& fhe,
    const std::string& label,
    size_t max_items = 8);

// 디버그용: ciphertext 기본 정보 출력
void print_cipher_basic_info(const seal::Ciphertext& cipher,
    const FHEContext& fhe,
    const std::string& label);

// 입력 하나를 encode -> encrypt -> decrypt -> decode 하면서
// 중간 상태를 전부 보여주는 디버그 함수
void debug_fhe_pipeline(const std::vector<uint64_t>& raw_input,
    const FHEContext& fhe,
    const std::string& label = "FHE Debug",
    size_t preview_count = 8);