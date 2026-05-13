#pragma once

#include <string>
#include <vector>
#include <cstdint>
using namespace std;

// CSV 로드 함수
vector<string> load_sender(const string& filepath);

// 미리보기 출력 함수
void preview_sender(const vector<string>& data, int n = 3);


// combined 문자열(주민번호13자리 + 질병10자리)을 uint64_t 하나로 패킹
uint64_t pack_combined_to_u64(const string& combined);
