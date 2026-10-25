#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace gtmeasy {
namespace internal {

bool is_click_provider(const std::string& provider);

struct ClickEntry {
  std::string value;
  int64_t ts = 0;
};

class ClickIdStore {
 public:
  explicit ClickIdStore(class Storage* storage);

  void load();
  int capture_url(const std::string& url);
  bool record(const std::string& provider, const std::string& value, std::string* error);
  void merge_into(nlohmann::json& ctx) const;
  const std::unordered_map<std::string, ClickEntry>& entries() const { return ids_; }
  // false = keep click ids in memory only (suppressed clients).
  void set_persist(bool persist) { persist_ = persist; }

 private:
  void prune();
  bool save(std::string* error);
  void derive_facebook(const std::string& fbclid);

  class Storage* storage_;
  std::unordered_map<std::string, ClickEntry> ids_;
  bool persist_ = true;
};

} // namespace internal
} // namespace gtmeasy
