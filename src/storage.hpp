#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace gtmeasy {
namespace internal {

struct QueueItem {
  std::string kind; // "event" | "user"
  nlohmann::json body;
  std::string raw_line;
};

class Storage {
 public:
  explicit Storage(std::string cache_dir);

  bool ensure_dir(std::string* error) const;

  bool load_or_create_id(const std::string& name, std::string* value, std::string* error);
  bool write_id(const std::string& name, const std::string& value, std::string* error);
  bool delete_id(const std::string& name, std::string* error);

  bool append_queue_line(const std::string& line, std::string* error);
  bool read_queue(std::vector<QueueItem>* items, std::string* error) const;
  bool rewrite_queue(const std::vector<QueueItem>& items, std::string* error);

  std::string path_for(const std::string& name) const;
  const std::string& cache_dir() const { return cache_dir_; }

  bool write_json(const std::string& name, const nlohmann::json& j, std::string* error);
  bool read_json(const std::string& name, nlohmann::json* j, std::string* error) const;

 private:
  std::string cache_dir_;
};

} // namespace internal
} // namespace gtmeasy
