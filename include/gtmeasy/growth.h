#ifndef GTMEASY_GROWTH_H
#define GTMEASY_GROWTH_H

#include "gtmeasy/growth_c.h"
#include "gtmeasy/version.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

#ifndef GTMEASY_NO_JSON
#include <nlohmann/json.hpp>
#endif

namespace gtmeasy {

inline gtm_error_t& last_error() {
  thread_local gtm_error_t err = GTM_OK;
  return err;
}

// Build channel of the host app. Never auto-detected: pass it from the app,
// e.g. `#ifndef NDEBUG` -> BuildChannel::Debug. Unknown is never suppressed.
enum class BuildChannel : int {
  Debug = GTM_BUILD_CHANNEL_DEBUG,
  TestFlight = GTM_BUILD_CHANNEL_TESTFLIGHT,
  InternalTest = GTM_BUILD_CHANNEL_INTERNAL_TEST,
  Production = GTM_BUILD_CHANNEL_PRODUCTION,
  Unknown = GTM_BUILD_CHANNEL_UNKNOWN,
};

struct Config {
  std::string apiKey;
  std::string app;
  std::string environment = "production";
  std::string baseUrl;
  std::string cacheDir;
  std::string appVersion;
  std::string buildNumber;
  int flushAt = 20;
  int flushIntervalMs = 30000;
  int maxQueueSize = 1000;
  int requestTimeoutMs = 10000;
  bool useBatchEndpoint = true;
  gtm_log_level_t logLevel = GTM_LOG_WARN;
  // Suppressed = disabled || (suppressTestBuilds && buildChannel is Debug,
  // TestFlight or InternalTest): nothing is sent or written to cacheDir.
  bool disabled = false;
  bool suppressTestBuilds = false;
  BuildChannel buildChannel = BuildChannel::Unknown;
};

inline std::string defaultCacheDir(const std::string& vendor, const std::string& app) {
#if defined(_WIN32)
  const char* base = std::getenv("LOCALAPPDATA");
  std::string root = base ? base : ".";
  return root + "\\" + vendor + "\\" + app + "\\gtmeasy";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  std::string root = home ? home : ".";
  return root + "/Library/Application Support/" + vendor + "/" + app + "/gtmeasy";
#else
  const char* xdg = std::getenv("XDG_DATA_HOME");
  if (xdg && xdg[0] != '\0') {
    return std::string(xdg) + "/" + vendor + "/" + app + "/gtmeasy";
  }
  const char* home = std::getenv("HOME");
  std::string root = home ? std::string(home) + "/.local/share" : ".";
  return root + "/" + vendor + "/" + app + "/gtmeasy";
#endif
}

class GrowthAnalytics {
 public:
  static std::unique_ptr<GrowthAnalytics> create(const Config& config) {
    gtm_growth_config_t c;
    gtm_growth_config_init(&c);
    c.api_key = config.apiKey.c_str();
    c.app = config.app.c_str();
    c.environment = config.environment.empty() ? nullptr : config.environment.c_str();
    c.base_url = config.baseUrl.empty() ? nullptr : config.baseUrl.c_str();
    c.cache_dir = config.cacheDir.c_str();
    c.app_version = config.appVersion.empty() ? nullptr : config.appVersion.c_str();
    c.build_number = config.buildNumber.empty() ? nullptr : config.buildNumber.c_str();
    c.flush_at = config.flushAt;
    c.flush_interval_ms = config.flushIntervalMs;
    c.max_queue_size = config.maxQueueSize;
    c.request_timeout_ms = config.requestTimeoutMs;
    c.use_batch_endpoint = config.useBatchEndpoint ? 1 : 0;
    c.log_level = config.logLevel;

    gtm_growth_config_ext_t ext;
    gtm_growth_config_ext_init(&ext);
    ext.disabled = config.disabled ? 1 : 0;
    ext.suppress_test_builds = config.suppressTestBuilds ? 1 : 0;
    ext.build_channel = static_cast<int>(config.buildChannel);

    gtm_growth_t* raw = nullptr;
    last_error() = gtm_growth_create_ext(&c, &ext, &raw);
    if (last_error() != GTM_OK || raw == nullptr) {
      return nullptr;
    }
    return std::unique_ptr<GrowthAnalytics>(new GrowthAnalytics(raw));
  }

  ~GrowthAnalytics() {
    if (client_) {
      gtm_growth_destroy(client_);
      client_ = nullptr;
    }
  }

  GrowthAnalytics(const GrowthAnalytics&) = delete;
  GrowthAnalytics& operator=(const GrowthAnalytics&) = delete;

#ifndef GTMEASY_NO_JSON
  gtm_error_t identify(const std::string& userId,
                       const nlohmann::json& traits = nlohmann::json::object(),
                       std::optional<std::string> username = {},
                       std::optional<std::string> email = {}) {
    if (!client_) {
      return GTM_ERROR_NOT_INITIALIZED;
    }
    const std::string traits_s = traits.dump();
    return gtm_growth_identify(client_,
                               userId.c_str(),
                               username ? username->c_str() : nullptr,
                               email ? email->c_str() : nullptr,
                               traits_s.c_str());
  }

  gtm_error_t track(const std::string& eventName,
                    const nlohmann::json& properties = nlohmann::json::object()) {
    if (!client_) {
      return GTM_ERROR_NOT_INITIALIZED;
    }
    const std::string props = properties.dump();
    return gtm_growth_track(client_, eventName.c_str(), props.c_str());
  }

  gtm_error_t trackMetric(const std::string& eventName,
                          double value,
                          const std::string& label,
                          const nlohmann::json& properties = nlohmann::json::object()) {
    if (!client_) {
      return GTM_ERROR_NOT_INITIALIZED;
    }
    const std::string props = properties.dump();
    return gtm_growth_track_metric(
        client_, eventName.c_str(), props.c_str(), value, label.c_str());
  }
#endif

  gtm_error_t trackFirstOpen() {
    if (!client_) {
      return GTM_ERROR_NOT_INITIALIZED;
    }
    return gtm_growth_track_first_open(client_);
  }

  gtm_error_t recordClickId(const std::string& provider, const std::string& value) {
    if (!client_) {
      return GTM_ERROR_NOT_INITIALIZED;
    }
    return gtm_growth_record_click_id(client_, provider.c_str(), value.c_str());
  }

  int captureClickIds(const std::string& url) {
    if (!client_) {
      return 0;
    }
    return gtm_growth_capture_click_ids(client_, url.c_str());
  }

  gtm_error_t flush(int timeoutMs = 5000) {
    if (!client_) {
      return GTM_ERROR_NOT_INITIALIZED;
    }
    return gtm_growth_flush(client_, timeoutMs);
  }

  gtm_error_t reset() {
    if (!client_) {
      return GTM_ERROR_NOT_INITIALIZED;
    }
    return gtm_growth_reset(client_);
  }

  // True when events are dropped (disabled, or a suppressed test build).
  bool isSuppressed() const { return client_ != nullptr && gtm_growth_is_suppressed(client_) != 0; }

  BuildChannel buildChannel() const {
    return client_ ? static_cast<BuildChannel>(gtm_growth_build_channel(client_))
                   : BuildChannel::Unknown;
  }

  std::string anonymousId() const {
    if (!client_) {
      return {};
    }
    const char* id = gtm_growth_anonymous_id(client_);
    return id ? std::string(id) : std::string();
  }

  std::optional<std::string> userId() const {
    if (!client_) {
      return std::nullopt;
    }
    const char* id = gtm_growth_user_id(client_);
    if (id == nullptr || id[0] == '\0') {
      return std::nullopt;
    }
    return std::string(id);
  }

  static const char* version() { return gtm_growth_version(); }

 private:
  explicit GrowthAnalytics(gtm_growth_t* client) : client_(client) {}
  gtm_growth_t* client_ = nullptr;
};

} // namespace gtmeasy

#endif
