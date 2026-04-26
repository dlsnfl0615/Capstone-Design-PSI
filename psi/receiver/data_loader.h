#pragma once

#include <string>
#include <vector>

// receiver 한 행의 데이터를 담는 구조체
struct ReceiverRecord {
    std::string combined; // 주민번호13자리 + 질병코드10자리 = 23자리
    std::string status;   // equal / disease_code_miss / person_id_miss
};

// CSV 로드 함수
std::vector<ReceiverRecord> load_receiver(const std::string& filepath);
std::vector<std::string> load_sender(const std::string& filepath);

// 미리보기 출력 함수
void preview_receiver(const std::vector<ReceiverRecord>& data, int n = 3);
void preview_sender(const std::vector<std::string>& data, int n = 3);


// combined 문자열(주민번호13자리 + 질병10자리)을 uint64_t 하나로 패킹
uint64_t pack_combined_to_u64(const std::string& combined);

// receiver 데이터를 FHE 입력 벡터로 변환
std::vector<uint64_t> make_receiver_input_u64(const std::vector<ReceiverRecord>& data,
                                              size_t max_items);
