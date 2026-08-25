#include "click_ids.hpp"

#include "storage.hpp"
#include "util.hpp"

#include <array>
#include <sstream>

namespace gtmeasy {
namespace internal {
namespace {

const std::array<const char*, 8> kProviders = {
    "gclid", "fbclid", "ttclid", "msclkid", "twclid", "wbraid", "gbraid", "igshid"};

constexpr int64_t kTtlSeconds = 90LL * 24LL * 60LL * 60LL;

std::string query_string(const std::string& url) {
  const auto q = url.find('?');
  if (q == std::string::npos) {
    if (url.find('=') != std::string::npos && url.find("://") == std::string::npos) {
      return url;
    }
    return {};
  }
  const auto hash = url.find('#', q);
  if (hash == std::string::npos) {
    return url.substr(q + 1);
  }
  return url.substr(q + 1, hash - q - 1);
}

} // namespace

bool is_click_provider(const std::string& provider) {
  const std::string lower = to_lower(provider);
  for (const char* p : kProviders) {
    if (lower == p) {
      return true;
    }
  }
  return false;
}

ClickIdStore::ClickIdStore(Storage* storage) : storage_(storage) {}

void ClickIdStore::load() {
  nlohmann::json j;
  if (!storage_->read_json("click_ids.json", &j, nullptr) || !j.is_object()) {
    return;
  }
  ids_.clear();
  for (auto it = j.begin(); it != j.end(); ++it) {
    ClickEntry e;
    if (it.value().is_object()) {
      e.value = it.value().value("value", "");
      e.ts = it.value().value("ts", static_cast<int64_t>(0));
    } else if (it.value().is_string()) {
      e.value = it.value().get<std::string>();
      e.ts = unix_s_now();
    }
    if (!e.value.empty()) {
      ids_[it.key()] = e;
    }
  }
  prune();
}

void ClickIdStore::prune() {
  const int64_t now = unix_s_now();
  for (auto it = ids_.begin(); it != ids_.end();) {
    if (it->second.ts > 0 && now - it->second.ts > kTtlSeconds) {
      it = ids_.erase(it);
    } else {
      ++it;
    }
  }
}

bool ClickIdStore::save(std::string* error) {
  nlohmann::json j = nlohmann::json::object();
  for (const auto& kv : ids_) {
    j[kv.first] = nlohmann::json{{"value", kv.second.value}, {"ts", kv.second.ts}};
  }
  return storage_->write_json("click_ids.json", j, error);
}

void ClickIdStore::derive_facebook(const std::string& fbclid) {
  const int64_t ms = unix_ms_now();
  ClickEntry fbc;
  fbc.value = "fb.1." + std::to_string(ms) + "." + fbclid;
  fbc.ts = unix_s_now();
  ids_["fbc"] = fbc;
  if (ids_.find("fbp") == ids_.end()) {
    ClickEntry fbp;
    fbp.value = "fb.1." + std::to_string(ms) + "." + random_digits(10);
    fbp.ts = fbc.ts;
    ids_["fbp"] = fbp;
  }
}

bool ClickIdStore::record(const std::string& provider, const std::string& value, std::string* error) {
  if (!is_click_provider(provider) || value.empty()) {
    if (error) {
      *error = "invalid click id provider";
    }
    return false;
  }
  const std::string key = to_lower(provider);
  ClickEntry e;
  e.value = value;
  e.ts = unix_s_now();
  ids_[key] = e;
  if (key == "fbclid") {
    derive_facebook(value);
  }
  return save(error);
}

int ClickIdStore::capture_url(const std::string& url) {
  const std::string qs = query_string(url);
  if (qs.empty()) {
    return 0;
  }
  int count = 0;
  std::stringstream ss(qs);
  std::string pair;
  while (std::getline(ss, pair, '&')) {
    if (pair.empty()) {
      continue;
    }
    const auto eq = pair.find('=');
    std::string key = eq == std::string::npos ? pair : pair.substr(0, eq);
    std::string val = eq == std::string::npos ? std::string() : pair.substr(eq + 1);
    key = to_lower(url_decode(key));
    val = url_decode(val);
    if (is_click_provider(key) && !val.empty()) {
      if (record(key, val, nullptr)) {
        ++count;
      }
    }
  }
  return count;
}

void ClickIdStore::merge_into(nlohmann::json& ctx) const {
  const int64_t now = unix_s_now();
  for (const auto& kv : ids_) {
    if (kv.second.ts > 0 && now - kv.second.ts > kTtlSeconds) {
      continue;
    }
    if (!kv.second.value.empty()) {
      ctx[kv.first] = kv.second.value;
    }
  }
}

} // namespace internal
} // namespace gtmeasy
