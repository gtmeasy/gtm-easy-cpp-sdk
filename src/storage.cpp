#include "storage.hpp"

#include "util.hpp"

#include <filesystem>
#include <fstream>

namespace gtmeasy {
namespace internal {

Storage::Storage(std::string cache_dir) : cache_dir_(std::move(cache_dir)) {}

std::string Storage::path_for(const std::string& name) const {
  namespace fs = std::filesystem;
  return (fs::path(cache_dir_) / name).string();
}

bool Storage::ensure_dir(std::string* error) const {
  std::error_code ec;
  std::filesystem::create_directories(cache_dir_, ec);
  if (ec) {
    if (error) {
      *error = "create_directories failed: " + ec.message();
    }
    return false;
  }
  return true;
}

bool Storage::load_or_create_id(const std::string& name, std::string* value, std::string* error) {
  const std::string path = path_for(name);
  std::string raw;
  if (read_file(path, &raw, nullptr)) {
    *value = trim(raw);
    if (!value->empty()) {
      return true;
    }
  }
  *value = uuid_v4();
  return write_id(name, *value, error);
}

bool Storage::write_id(const std::string& name, const std::string& value, std::string* error) {
  return atomic_write_file(path_for(name), value + "\n", error);
}

bool Storage::delete_id(const std::string& name, std::string* error) {
  std::error_code ec;
  std::filesystem::remove(path_for(name), ec);
  if (ec && error) {
    *error = ec.message();
    return false;
  }
  (void)error;
  return true;
}

bool Storage::append_queue_line(const std::string& line, std::string* error) {
  const std::string path = path_for("queue.jsonl");
  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    if (error) {
      *error = "failed to append queue.jsonl";
    }
    return false;
  }
  out << line;
  if (line.empty() || line.back() != '\n') {
    out << '\n';
  }
  return static_cast<bool>(out);
}

static QueueItem parse_line(const std::string& line) {
  QueueItem item;
  item.raw_line = line;
  auto j = nlohmann::json::parse(line, nullptr, false);
  if (j.is_discarded() || !j.is_object()) {
    item.kind = "event";
    item.body = nlohmann::json::object();
    return item;
  }
  item.kind = j.value("kind", "event");
  if (j.contains("body") && j["body"].is_object()) {
    item.body = j["body"];
  } else {
    item.body = nlohmann::json::object();
  }
  return item;
}

bool Storage::read_queue(std::vector<QueueItem>* items, std::string* error) const {
  items->clear();
  const std::string path = path_for("queue.jsonl");
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return true;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (trim(line).empty()) {
      continue;
    }
    items->push_back(parse_line(line));
  }
  (void)error;
  return true;
}

bool Storage::rewrite_queue(const std::vector<QueueItem>& items, std::string* error) {
  std::string contents;
  for (const auto& item : items) {
    nlohmann::json line;
    line["kind"] = item.kind;
    line["body"] = item.body;
    contents += line.dump();
    contents += '\n';
  }
  return atomic_write_file(path_for("queue.jsonl"), contents, error);
}

bool Storage::write_json(const std::string& name, const nlohmann::json& j, std::string* error) {
  return atomic_write_file(path_for(name), j.dump(2), error);
}

bool Storage::read_json(const std::string& name, nlohmann::json* j, std::string* error) const {
  std::string raw;
  if (!read_file(path_for(name), &raw, error)) {
    return false;
  }
  auto parsed = nlohmann::json::parse(raw, nullptr, false);
  if (parsed.is_discarded()) {
    if (error) {
      *error = "invalid json in " + name;
    }
    return false;
  }
  *j = std::move(parsed);
  return true;
}

} // namespace internal
} // namespace gtmeasy
