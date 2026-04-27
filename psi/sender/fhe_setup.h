#pragma once

#include "seal/seal.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <fstream> // 파일 스트림 처리를 위해 필요

class FHESetup {
public:
    // SEAL 핵심 객체들을 저장할 멤버 변수
    std::shared_ptr<seal::SEALContext> context;
    seal::PublicKey public_key;
    seal::RelinKeys relin_keys;

    // 송신자는 복호화 기능이 필요 없으므로 SecretKey와 Decryptor는 제외함

    // unique_ptr을 사용하여 setup_sender 호출 시 객체 생성
    std::unique_ptr<seal::Encryptor> encryptor;
    std::unique_ptr<seal::Evaluator> evaluator;
    std::unique_ptr<seal::BatchEncoder> batch_encoder;

    /**
     * @brief 수신자로부터 받은 키 파일을 로드하여 송신자 환경을 설정함
     * @param n poly_modulus_degree (수신자와 반드시 일치해야 함)
     * @param t_bits plain_modulus의 비트 크기
     * @param pk_path 공개키 파일 경로
     * @param rl_path 릴리니어라이제이션 키 파일 경로
     */
    void setup_sender(size_t n, int t_bits, const std::string& pk_path, const std::string& rl_path);
};