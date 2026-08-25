#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gtmeasy {
namespace internal {

std::string uuid_v4();
std::string rfc3339_now();
int64_t unix_ms_now();
int64_t unix_s_now();
double backoff_seconds(int attempt);
std::string trim(const std::string& s);
std::string to_lower(std::string s);
std::string url_decode(const std::string& s);
std::string join_url(const std::string& base, const std::string& path);
bool atomic_write_file(const std::string& path, const std::string& contents, std::string* error);
bool read_file(const std::string& path, std::string* out, std::string* error);
bool write_text_file(const std::string& path, const std::string& contents, std::string* error);
std::string random_digits(int n);
int parse_retry_after(const std::string& header, int fallback_seconds);

} // namespace internal
} // namespace gtmeasy
