#include <iostream>
#include <vector>
#include <string>
#include "psi_sender.h"
#include "hashing.h"
#include "fhe_setup.h"
#include "data_loader.h"
#include "../parameters.h"

using namespace std;

int main() {
    try {
        // [Step 1] 환경 설정 및 데이터 로드
        cout << "[Step 1] Sender: Environment setup and Loading data..." << endl;
        
        // FHESetup을 통해 수신자의 공개키와 릴리니어라이제이션 키 로드
        FHESetup fhe;
        // 주의: 실행 경로에 public_key.bin, relin_keys.bin 파일이 있어야 합니다.
        fhe.setup_sender(n, 44, "public_key.bin", "relin_keys.bin");

        // PsiSender를 통해 수신자가 보낸 거듭제곱 암호문(windowed powers) 로드
        PsiSender psi_sender;
        // 주의: 실행 경로에 수신자가 생성한 powers.bin 파일이 있어야 합니다.
        psi_sender.load_receiver_powers("powers.bin", *fhe.context);

        // 테스트용 더미 데이터 생성 (13자리 주민번호 + 10자리 2진수 질병코드 = 총 23자)
        // SimpleHashing::locate의 파싱 로직에 맞춘 형식입니다.
        auto sender_data = load_sender("data/sender.csv");

        // [Step 2] 단순 해싱 수행
        cout << "\n[Step 2] Performing Simple Hashing..." << endl;
        SimpleHashing simple_hashing;
        
        // 아이템들을 해시 테이블의 h개 가능한 모든 위치에 배치
        // 순열 기반 해싱(Permutation-based hashing)을 통해 비트 분리 및 XOR 연산 수행
        simple_hashing.locate(sender_data);

        // [Step 3] 파티셔닝 수행
        cout << "\n[Step 3] Partitioning Bins into alpha subsets..." << endl;
        // SimpleHashing의 결과물을 사용하여 파티션 생성
        psi_sender.partition_bins(simple_hashing.hash_table);

        // [Step 4] 다항식 계수 미리 계산
        cout << "\n[Step 4] Computing and batching polynomial coefficients..." << endl;
        
        // plain_modulus 값은 context의 first_context_data에서 가져옴
        uint64_t t = fhe.context->first_context_data()->parms().plain_modulus().value();
        
        psi_sender.compute_coefficients(*fhe.batch_encoder, t);

        // [Step 5] 동형 다항식 평가 (Online)
        cout << "\n[Step 5] Homomorphically evaluating polynomials..." << endl;
        
        // 릴리니어라이제이션 키와 에밸루에이터를 사용하여 연산 수행
        psi_sender.evaluate_polynomials(*fhe.evaluator, fhe.relin_keys);

        // [Step 6] 무작위화 및 응답 저장
        cout << "\n[Step 6] Randomizing results and saving to file..." << endl;
        
        // 무작위 값을 곱해 결과 마스킹 후 response.bin 생성
        psi_sender.randomize_and_save_responses(
            *fhe.evaluator, 
            *fhe.batch_encoder, 
            t, 
            "response.bin"
        );

        cout << "\n[Success] All Sender steps are completed." << endl;
        cout << "Please send 'response.bin' back to the Receiver." << endl;

    } catch (const std::exception& e) {
        cerr << "\n[Error] Exception occurred: " << e.what() << endl;
        return 1;
    }
    return 0;
}