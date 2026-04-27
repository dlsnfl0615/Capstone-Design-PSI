#include "fhe_setup.h"

void FHESetup::setup_sender(size_t n, int t_bits, const std::string& pk_path, const std::string& rl_path) {
    // 1. 파라미터 설정 (수신자의 설정과 반드시 동일해야 함)
    seal::EncryptionParameters parms(seal::scheme_type::bfv);
    parms.set_poly_modulus_degree(n);
    parms.set_coeff_modulus(seal::CoeffModulus::BFVDefault(n));
    parms.set_plain_modulus(seal::PlainModulus::Batching(n, t_bits));

    // 2. 컨텍스트 생성 및 유효성 검사
    context = std::make_shared<seal::SEALContext>(parms);
    if (!context->parameters_set()) {
        throw std::runtime_error(std::string("SEAL parameters error: ") + context->parameter_error_message());
    }

    // 3. 파일로부터 공개키와 릴리니어라이제이션 키 로드
    std::ifstream fs_pk(pk_path, std::ios::binary);
    if (!fs_pk.is_open()) throw std::runtime_error("Cannot open public key file: " + pk_path);
    public_key.load(*context, fs_pk); // 수신자가 생성한 pk 로드

    std::ifstream fs_rl(rl_path, std::ios::binary);
    if (!fs_rl.is_open()) throw std::runtime_error("Cannot open relin keys file: " + rl_path);
    relin_keys.load(*context, fs_rl); // 수신자가 생성한 rl 로드

    // 4. 연산 및 인코딩 객체 초기화
    encryptor = std::make_unique<seal::Encryptor>(*context, public_key);
    evaluator = std::make_unique<seal::Evaluator>(*context);
    batch_encoder = std::make_unique<seal::BatchEncoder>(*context);
    
    // 송신자는 Decryptor를 생성하지 않음 (보안 원칙 준수)
}