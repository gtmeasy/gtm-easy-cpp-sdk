#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>

#include <filesystem>

namespace gtmeasy {
namespace internal {
namespace {

std::mt19937_64& rng() {
  static thread_local std::mt19937_64 gen([] {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(),
                      static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count())};
    return std::mt19937_64(seq);
  }());
  return gen;
}

} // namespace

std::string uuid_v4() {
  std::uniform_int_distribution<uint32_t> dist(0, 255);
  unsigned char bytes[16];
  for (unsigned char& b : bytes) {
    b = static_cast<unsigned char>(dist(rng()));
  }
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.resize(36);
  int o = 0;
  auto put = [&](int i) {
    out[o++] = hex[(bytes[i] >> 4) & 0xf];
    out[o++] = hex[bytes[i] & 0xf];
  };
  put(0);
  put(1);
  put(2);
  put(3);
  out[o++] = '-';
  put(4);
  put(5);
  out[o++] = '-';
  put(6);
  put(7);
  out[o++] = '-';
  put(8);
  put(9);
  out[o++] = '-';
  put(10);
  put(11);
  put(12);
  put(13);
  put(14);
  put(15);
  return out;
}

int64_t unix_ms_now() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

int64_t unix_s_now() {
  return unix_ms_now() / 1000;
}

std::string rfc3339_now() {
  const auto ms = unix_ms_now();
  const std::time_t secs = static_cast<std::time_t>(ms / 1000);
  const int millis = static_cast<int>(ms % 1000);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &secs);
#else
  gmtime_r(&secs, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
      << millis << 'Z';
  return oss.str();
}

double backoff_seconds(int attempt) {
  const double base = (std::min)(60.0, std::ldexp(1.0, (std::max)(0, attempt)));
  std::uniform_real_distribution<double> dist(0.8, 1.2);
  return base * dist(rng());
}

std::string trim(const std::string& s) {
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) {
    ++a;
  }
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
    --b;
  }
  return s.substr(a, b - a);
}

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

std::string url_decode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '+') {
      out.push_back(' ');
    } else if (s[i] == '%' && i + 2 < s.size()) {
      auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') {
          return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
          return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
          return c - 'A' + 10;
        }
        return -1;
      };
      const int hi = hex(s[i + 1]);
      const int lo = hex(s[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
      } else {
        out.push_back(s[i]);
      }
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

std::string join_url(const std::string& base, const std::string& path) {
  if (base.empty()) {
    return path;
  }
  if (!base.empty() && base.back() == '/' && !path.empty() && path.front() == '/') {
    return base + path.substr(1);
  }
  if (!base.empty() && base.back() != '/' && !path.empty() && path.front() != '/') {
    return base + "/" + path;
  }
  return base + path;
}

bool write_text_file(const std::string& path, const std::string& contents, std::string* error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    if (error) {
      *error = "failed to open " + path;
    }
    return false;
  }
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  if (!out) {
    if (error) {
      *error = "failed to write " + path;
    }
    return false;
  }
  return true;
}

bool atomic_write_file(const std::string& path, const std::string& contents, std::string* error) {
  namespace fs = std::filesystem;
  const fs::path dest(path);
  const fs::path tmp = dest.string() + ".tmp";
  if (!write_text_file(tmp.string(), contents, error)) {
    return false;
  }
  std::error_code ec;
  fs::rename(tmp, dest, ec);
  if (ec) {
    fs::remove(dest, ec);
    fs::rename(tmp, dest, ec);
    if (ec) {
      if (error) {
        *error = "rename failed for " + path + ": " + ec.message();
      }
      fs::remove(tmp, ec);
      return false;
    }
  }
  return true;
}

bool read_file(const std::string& path, std::string* out, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) {
      *error = "failed to open " + path;
    }
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  return true;
}

std::string random_digits(int n) {
  std::uniform_int_distribution<int> dist(0, 9);
  std::string s;
  s.resize(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    s[static_cast<size_t>(i)] = static_cast<char>('0' + dist(rng()));
  }
  return s;
}

int parse_retry_after(const std::string& header, int fallback_seconds) {
  if (header.empty()) {
    return fallback_seconds;
  }
  try {
    const int v = std::stoi(trim(header));
    return v > 0 ? v : fallback_seconds;
  } catch (...) {
    return fallback_seconds;
  }
}

} // namespace internal
} // namespace gtmeasy
