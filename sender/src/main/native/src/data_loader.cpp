#include "data_loader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

// -----------------------------
// 내부 보조 함수들
// -----------------------------

// CSV 한 줄을 쉼표 기준으로 분리
static vector<string> split(const string& line, char delim = ',') {
    vector<string> tokens;
    stringstream ss(line);
    string tok;

    while (getline(ss, tok, delim)) {
        // 따옴표 제거
        tok.erase(remove(tok.begin(), tok.end(), '"'), tok.end());

        // Windows 줄끝 문자 제거
        tok.erase(remove(tok.begin(), tok.end(), '\r'), tok.end());

        // 앞 공백 제거
        auto first = tok.find_first_not_of(" \t");
        if (first == string::npos) {
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
static string normalize_pid(string pid) {
    pid.erase(remove(pid.begin(), pid.end(), '-'), pid.end());
    return pid;
}

// -----------------------------
// Receiver CSV 로드
// 컬럼: personal_id(0), Disease_1~10(1~10), Status(11)
// -----------------------------

std::vector<std::string> load_sender(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    std::vector<std::string> result;
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

        if (t.size() < 11) {
            throw std::runtime_error(
                "Sender " + std::to_string(line_num) + " not enough columns "
                "(expected: 11, actual: " + std::to_string(t.size()) + ")\n"
                "  content: " + line
            );
        }

        std::string pid = normalize_pid(t[0]);

        std::string disease = "";
        for (int i = 1; i <= 10; i++) {
            disease += t[i];
        }

        result.push_back(pid + disease);
    }

    std::cout << "[load]" << result.size() << " records loaded\n\n";
    return result;
}

// -----------------------------
// combined 문자열을 uint64_t로 패킹
// 형식: [주민번호13자리 decimal][질병10자리 binary]
// packed = (pid_decimal << 10) | disease_bits
// -----------------------------
uint64_t pack_combined_to_u64(const string& combined) {
    if (combined.size() != 23) {
        throw runtime_error(
            "combined length must be 23, got: " + to_string(combined.size()) +
            " / value: " + combined
        );
    }

    string pid_str = combined.substr(0, 13);     // 주민번호 13자리
    string dis_str = combined.substr(13, 10);    // 질병 10자리 (0/1)

    // 주민번호 13자리를 decimal 정수로 변환
    uint64_t pid_value = 0;
    for (char c : pid_str) {
        if (c < '0' || c > '9') {
            throw runtime_error("invalid personal_id digit in combined: " + combined);
        }
        pid_value = pid_value * 10 + static_cast<uint64_t>(c - '0');
    }

    // 질병 10자리를 binary 정수로 변환
    uint64_t disease_bits = 0;
    for (char c : dis_str) {
        if (c != '0' && c != '1') {
            throw runtime_error("invalid disease bit in combined: " + combined);
        }
        disease_bits = (disease_bits << 1) | static_cast<uint64_t>(c - '0');
    }

    // 최종 패킹: 주민번호 값 << 10 | 질병 10비트
    return (pid_value << 10) | disease_bits;
}