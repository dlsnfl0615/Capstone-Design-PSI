#include "fhe_setup.h"

void FHESetup::setup(size_t n, int t_bits) {
    // 1. 파라미터 설정
    seal::EncryptionParameters parms(seal::scheme_type::bfv);
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(seal::CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(seal::PlainModulus::Batching(n, t_bits));

    // 2. 컨텍스트 생성 및 유효성 검사
    context = std::make_shared<seal::SEALContext>(parms);
    if (!context->parameters_set()) {
        // 수정: 'parameters_error_message' -> 'parameter_error_message' (단수형)
        // 수정: 문자열 결합을 위해 std::string()으로 감싸줌
        throw std::runtime_error(std::string("SEAL parameters error: ") + context->parameter_error_message());
    }

    // 3. 키 생성
    seal::KeyGenerator keygen(*context);
    secret_key = keygen.secret_key();
    keygen.create_public_key(public_key);
    keygen.create_relin_keys(relin_keys);

    // 4. 연산 및 인코딩 객체 초기화
    encryptor = std::make_unique<seal::Encryptor>(*context, public_key);
    evaluator = std::make_unique<seal::Evaluator>(*context);
    batch_encoder = std::make_unique<seal::BatchEncoder>(*context);
    decryptor = std::make_unique<seal::Decryptor>(*context, secret_key);
}

void FHESetup::setup_for_response(size_t n, int t_bits, const std::string& sk_path) {
    // 파라미터 및 컨텍스트 설정 (기존과 동일)
    seal::EncryptionParameters parms(seal::scheme_type::bfv);
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(seal::CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(seal::PlainModulus::Batching(n, t_bits));
    context = std::make_shared<seal::SEALContext>(parms);

    // 비밀키 로드
    std::ifstream fs_sk(sk_path, std::ios::binary);
    secret_key.load(*context, fs_sk);

    // 복호화 및 인코딩 객체만 있으면 됨
    decryptor = std::make_unique<seal::Decryptor>(*context, secret_key);
    batch_encoder = std::make_unique<seal::BatchEncoder>(*context);
}