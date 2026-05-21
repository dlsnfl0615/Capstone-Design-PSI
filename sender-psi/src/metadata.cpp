#include "metadata.h"
#include "parameters.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <map>

using namespace std;

static string current_time_string() {
    time_t now = time(nullptr);
    tm* local_tm = localtime(&now);

    stringstream ss;
    ss << put_time(local_tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

SenderCacheMeta make_current_sender_meta(const string& table_file) {
    SenderCacheMeta meta;

    meta.m_value = m;
    meta.B_value = B;
    meta.h_value = h;
    meta.n_value = n;
    meta.num_blocks_value = num_blocks;
    meta.alpha_value = alpha;
    meta.B_prime_value = B_prime;
    meta.sigma_value = sigma;
    meta.sender_dummy_value = SENDER_DUMMY;
    meta.receiver_dummy_value = RECEIVER_DUMMY;
    meta.table_file = table_file;
    meta.created_at = current_time_string();

    return meta;
}

void save_sender_meta(
    const SenderCacheMeta& meta,
    const string& filepath
) {
    ofstream out(filepath);

    if (!out.is_open()) {
        throw runtime_error("Failed to open sender meta file for write: " + filepath);
    }

    out << "m=" << meta.m_value << "\n";
    out << "B=" << meta.B_value << "\n";
    out << "h=" << meta.h_value << "\n";
    out << "n=" << meta.n_value << "\n";
    out << "num_blocks=" << meta.num_blocks_value << "\n";
    out << "alpha=" << meta.alpha_value << "\n";
    out << "B_prime=" << meta.B_prime_value << "\n";
    out << "sigma=" << meta.sigma_value << "\n";
    out << "sender_dummy=" << meta.sender_dummy_value << "\n";
    out << "receiver_dummy=" << meta.receiver_dummy_value << "\n";
    out << "table_file=" << meta.table_file << "\n";
    out << "created_at=" << meta.created_at << "\n";

    out.close();

    cout << "[meta] sender meta saved: " << filepath << endl;
}

static map<string, string> read_key_value_file(const string& filepath) {
    ifstream in(filepath);

    if (!in.is_open()) {
        throw runtime_error("Failed to open sender meta file for read: " + filepath);
    }

    map<string, string> kv;
    string line;

    while (getline(in, line)) {
        if (line.empty()) continue;

        size_t pos = line.find('=');
        if (pos == string::npos) continue;

        string key = line.substr(0, pos);
        string value = line.substr(pos + 1);

        kv[key] = value;
    }

    in.close();
    return kv;
}

SenderCacheMeta load_sender_meta(
    const string& filepath
) {
    auto kv = read_key_value_file(filepath);

    SenderCacheMeta meta;

    meta.m_value = stoi(kv.at("m"));
    meta.B_value = stoi(kv.at("B"));
    meta.h_value = stoi(kv.at("h"));
    meta.n_value = stoi(kv.at("n"));
    meta.num_blocks_value = stoi(kv.at("num_blocks"));
    meta.alpha_value = stoi(kv.at("alpha"));
    meta.B_prime_value = stoi(kv.at("B_prime"));
    meta.sigma_value = stoull(kv.at("sigma"));
    meta.sender_dummy_value = stoull(kv.at("sender_dummy"));
    meta.receiver_dummy_value = stoull(kv.at("receiver_dummy"));
    meta.table_file = kv.at("table_file");
    meta.created_at = kv.at("created_at");

    cout << "[meta] sender meta loaded: " << filepath << endl;
    cout << "[meta] cache created_at = " << meta.created_at << endl;

    return meta;
}

void validate_sender_meta(
    const SenderCacheMeta& meta
) {
    if (meta.m_value != m) {
        throw runtime_error("Meta mismatch: m");
    }

    if (meta.B_value != B) {
        throw runtime_error("Meta mismatch: B");
    }

    if (meta.h_value != h) {
        throw runtime_error("Meta mismatch: h");
    }

    if (meta.n_value != n) {
        throw runtime_error("Meta mismatch: n");
    }

    if (meta.num_blocks_value != num_blocks) {
        throw runtime_error("Meta mismatch: num_blocks");
    }

    if (meta.alpha_value != alpha) {
        throw runtime_error("Meta mismatch: alpha");
    }

    if (meta.B_prime_value != B_prime) {
        throw runtime_error("Meta mismatch: B_prime");
    }

    if (meta.sigma_value != sigma) {
        throw runtime_error("Meta mismatch: sigma");
    }

    if (meta.sender_dummy_value != SENDER_DUMMY) {
        throw runtime_error("Meta mismatch: SENDER_DUMMY");
    }

    if (meta.receiver_dummy_value != RECEIVER_DUMMY) {
        throw runtime_error("Meta mismatch: RECEIVER_DUMMY");
    }

    cout << "[meta] sender cache metadata validation passed" << endl;
}