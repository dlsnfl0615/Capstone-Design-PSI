#pragma once

#include <string>
#include <cstdint>
#include <vector>
using namespace std;

// CSV 로드 함수
vector<string> load_receiver(const string& filepath);
vector<string> load_sender(const string& filepath);

// 미리보기 출력 함수
void preview_receiver(const vector<string>& data, int n = 3);
void preview_sender(const vector<string>& data, int n = 3);


// combined 문자열(주민번호13자리 + 질병10자리)을 uint64_t 하나로 패킹
uint64_t pack_combined_to_u64(const string& combined);

// receiver 데이터를 FHE 입력 벡터로 변환
vector<uint64_t> make_receiver_input_u64(const vector<string>& data,
                                              size_t max_items);
