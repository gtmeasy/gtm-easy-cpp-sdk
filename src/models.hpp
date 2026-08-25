#pragma once

#include "gtmeasy/version.h"

#include "context.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace gtmeasy {
namespace internal {

inline void set_optional_string(nlohmann::json& j, const char* key, const std::string& value) {
  if (!value.empty()) {
    j[key] = value;
  }
}

inline nlohmann::json envelope_base(const std::string& app,
                                    const std::string& environment,
                                    const std::string& platform,
                                    const std::string& user_id,
                                    const std::string& anonymous_id,
                                    const std::string& device_id,
                                    const std::string& app_version,
                                    const std::string& build_number,
                                    const std::string& locale,
                                    const std::string& timezone) {
  nlohmann::json j;
  j["app"] = app;
  j["environment"] = environment.empty() ? "production" : environment;
  j["platform"] = platform;
  if (!user_id.empty()) {
    j["userId"] = user_id;
  }
  j["anonymousId"] = anonymous_id;
  if (!device_id.empty()) {
    j["deviceId"] = device_id;
  }
  set_optional_string(j, "appVersion", app_version);
  set_optional_string(j, "buildNumber", build_number);
  set_optional_string(j, "locale", locale);
  set_optional_string(j, "timezone", timezone);
  return j;
}

} // namespace internal
} // namespace gtmeasy
