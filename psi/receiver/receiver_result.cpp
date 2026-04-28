#include <iostream>
#include <fstream>
#include "fhe_setup.h"
#include "psi_receiver.h"
#include "../parameters.h"
#include "hashing.h"

using namespace std;

int main() {
    // 1. 환경 설정 (1단계와 동일한 n, t 사용)
    FHESetup fhe;
    fhe.setup_for_response(n, t, "secret_key.bin"); // 비밀키 로드 함수 필요

    // 2. 해시 테이블 로드 (인덱스 복구용)
    Hashing hashing;
    ifstream ht_file("hash_table.bin", ios::binary);
    for(int i=0; i<m; i++) {
        ht_file.read(reinterpret_cast<char*>(&hashing.hash_table[i]), sizeof(uint64_t));
    }

    // 3. 송신자 응답 로드 및 처리
    PsiReceiver psi_receiver;
    auto responses = psi_receiver.load_responses("response.bin", *fhe.context);
    auto intersection_indices = psi_receiver.identify_intersection(responses, *fhe.decryptor, *fhe.batch_encoder);

    // 4. 최종 결과 출력
    for (int idx : intersection_indices) {
        RestoredData res = hashing.restore_original_data(hashing.hash_table[idx], idx);
        cout << "Found Intersection - PID: " << res.pid << ", Disease: " << res.disease << endl;
    }

    cout << intersection_indices.size() << endl;
}