#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace gtmeasy {
namespace internal {

struct DeviceContext {
  std::string sdk = "cpp";
  std::string sdk_version;
  std::string platform;
  std::string os_version;
  std::string device_manufacturer;
  std::string device_model;
  uint64_t physical_memory_bytes = 0;
  unsigned hw_concurrency = 0;
  std::string locale;
  std::string timezone;
};

DeviceContext snapshot_context();
nlohmann::json context_to_json(const DeviceContext& ctx);
const char* platform_name();

} // namespace internal
} // namespace gtmeasy
