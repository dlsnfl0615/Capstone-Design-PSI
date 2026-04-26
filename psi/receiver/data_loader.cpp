#include "data_loader.h"
#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

// -----------------------------
// 내부 보조 함수들
// -----------------------------

// CSV 한 줄을 쉼표 기준으로 분리
static std::vector<std::string> split(const std::string& line, char delim = ',') {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string tok;

    while (std::getline(ss, tok, delim)) {
        // 따옴표 제거
        tok.erase(std::remove(tok.begin(), tok.end(), '"'), tok.end());

        // Windows 줄끝 문자 제거
        tok.erase(std::remove(tok.begin(), tok.end(), '\r'), tok.end());

        // 앞 공백 제거
        auto first = tok.find_first_not_of(" \t");
        if (first == std::string::npos) {
            tok.clear();
        } else {
            tok.erase(0, first);

            // 뒤 공백 제거
            auto last = tok.find_last_not_of(" \t");
            tok.erase(last + 1);
        }

        tokens.push_back(tok);
    }
    return tokens;
}

// personal_id에서 하이픈 제거
// 예: "920624-2447527" -> "9206242447527"
static std::string normalize_pid(std::string pid) {
    pid.erase(std::remove(pid.begin(), pid.end(), '-'), pid.end());
    return pid;
}

// -----------------------------
// Receiver CSV 로드
// 컬럼: personal_id(0), Disease_1~10(1~10), Status(11)
// -----------------------------
std::vector<ReceiverRecord> load_receiver(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        throw std::runtime_error("fail to open file: " + filepath);
    }

    std::vector<ReceiverRecord> result;
    std::string line;
    bool header = true;
    int line_num = 0;

    while (std::getline(f, line)) {
        ++line_num;

        if (header) {
            header = false;
            continue;
        }

        if (line.empty()) continue;

        auto t = split(line);

        if (t.size() < 12) {
            throw std::runtime_error(
                "Receiver " + std::to_string(line_num) + "th columns has less data "
                "(expected: 12, real: " + std::to_string(t.size()) + ")\n"
                "  contains: " + line
            );
        }

        std::string pid = normalize_pid(t[0]);

        std::string disease = "";
        for (int i = 1; i <= 10; i++) {
            disease += t[i];
        }

        std::string status = t[11];

        result.push_back({ pid + disease, status });
    }

    std::cout << "[Receiver] " << result.size() << " records loaded\n";
    return result;
}

// -----------------------------
// 미리보기 출력
// -----------------------------
void preview_receiver(const std::vector<ReceiverRecord>& data, int n) {
    std::cout << "\n--- Receiver preview ---\n";
    for (int i = 0; i < std::min(n, (int)data.size()); i++) {
        std::cout << "  [" << i << "] " << data[i].combined
                  << "  (" << data[i].combined.size() << " chars)"
                  << "  status=" << data[i].status << "\n";
    }
}

//0330 1:27 추가 : fhe_setup 검증을 위해 csv 데이터들을 벡터로 변환하는 함수 추가
// -----------------------------
// combined 문자열을 uint64_t로 패킹
// 형식: [주민번호13자리 decimal][질병10자리 binary]
// packed = (pid_decimal << 10) | disease_bits
// -----------------------------
uint64_t pack_combined_to_u64(const std::string& combined) {
    if (combined.size() != 23) {
        throw std::runtime_error(
            "combined length must be 23, got: " + std::to_string(combined.size()) +
            " / value: " + combined
        );
    }

    std::string pid_str = combined.substr(0, 13);     // 주민번호 13자리
    std::string dis_str = combined.substr(13, 10);    // 질병 10자리 (0/1)

    // 주민번호 13자리를 decimal 정수로 변환
    uint64_t pid_value = 0;
    for (char c : pid_str) {
        if (c < '0' || c > '9') {
            throw std::runtime_error("invalid personal_id digit in combined: " + combined);
        }
        pid_value = pid_value * 10 + static_cast<uint64_t>(c - '0');
    }

    // 질병 10자리를 binary 정수로 변환
    uint64_t disease_bits = 0;
    for (char c : dis_str) {
        if (c != '0' && c != '1') {
            throw std::runtime_error("invalid disease bit in combined: " + combined);
        }
        disease_bits = (disease_bits << 1) | static_cast<uint64_t>(c - '0');
    }

    // 최종 패킹: 주민번호 값 << 10 | 질병 10비트
    return (pid_value << 10) | disease_bits;
}

// -----------------------------
// receiver -> uint64_t 벡터
// max_items개까지만 사용
// -----------------------------
std::vector<uint64_t> make_receiver_input_u64(const std::vector<ReceiverRecord>& data,
                                              size_t max_items) {
    std::vector<uint64_t> out;
    out.reserve(std::min(max_items, data.size()));

    for (size_t i = 0; i < data.size() && i < max_items; i++) {
        out.push_back(pack_combined_to_u64(data[i].combined));
    }
    return out;
}
