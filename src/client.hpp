#pragma once

#include "gtmeasy/growth_c.h"

#include "click_ids.hpp"
#include "context.hpp"
#include "queue.hpp"
#include "storage.hpp"
#include "transport.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

struct gtm_growth {
  std::string api_key;
  std::string app;
  std::string environment;
  std::string base_url;
  std::string cache_dir;
  std::string app_version;
  std::string build_number;
  int flush_at = 20;
  int flush_interval_ms = 30000;
  int max_queue_size = 1000;
  int request_timeout_ms = 10000;
  bool use_batch = true;
  gtm_log_level_t log_level = GTM_LOG_WARN;
  gtm_log_fn log_fn = nullptr;
  void* log_user_data = nullptr;
  // Set once at create; never changes. When true: no network, no worker
  // thread, no writes to cache_dir.
  bool suppressed = false;
  gtm_build_channel_t build_channel = GTM_BUILD_CHANNEL_UNKNOWN;

  gtmeasy::internal::Storage storage;
  gtmeasy::internal::Queue queue;
  gtmeasy::internal::ClickIdStore clicks;
  gtmeasy::internal::DeviceContext device;
  std::unique_ptr<gtmeasy::internal::Transport> transport;

  std::string anonymous_id;
  std::string device_id;
  std::string user_id;

  std::mutex mu;
  std::mutex flush_mu;
  std::condition_variable cv;
  std::thread worker;
  std::atomic<bool> stop{false};
  std::atomic<bool> flush_requested{false};
  std::atomic<bool> auth_failed{false};
  std::atomic<bool> worker_started{false};

  gtm_growth(std::string cache)
      : storage(std::move(cache)),
        queue(&storage, 1000),
        clicks(&storage) {}
};

namespace gtmeasy {
namespace internal {

void client_log(gtm_growth* c, gtm_log_level_t level, const std::string& message);
nlohmann::json parse_object_json(const char* json, gtm_error_t* err);
nlohmann::json build_ctx(gtm_growth* c);
nlohmann::json build_event(gtm_growth* c,
                           const std::string& event_name,
                           nlohmann::json properties,
                           bool has_metric,
                           double metric_value,
                           const char* metric_label);
nlohmann::json build_identify(gtm_growth* c,
                              const char* user_id,
                              const char* username,
                              const char* email,
                              nlohmann::json traits);

const char* build_channel_name(gtm_build_channel_t channel);
bool compute_suppressed(bool disabled, bool suppress_test_builds, gtm_build_channel_t channel);

void worker_main(gtm_growth* c);
void run_flush_cycle(gtm_growth* c, int timeout_ms_budget);
void reset_batch_supported_for_tests();

} // namespace internal
} // namespace gtmeasy
