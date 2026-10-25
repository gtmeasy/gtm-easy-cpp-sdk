// Client lifecycle: config defaults, create (+ versioned ext), destroy, and the
// build-channel suppression rule.
#include "client.hpp"

#include "util.hpp"

#include <cstddef>
#include <new>

namespace gtmeasy {
namespace internal {

const char* build_channel_name(gtm_build_channel_t channel) {
  switch (channel) {
  case GTM_BUILD_CHANNEL_DEBUG:
    return "debug";
  case GTM_BUILD_CHANNEL_TESTFLIGHT:
    return "testflight";
  case GTM_BUILD_CHANNEL_INTERNAL_TEST:
    return "internalTest";
  case GTM_BUILD_CHANNEL_PRODUCTION:
    return "production";
  default:
    return "unknown";
  }
}

bool compute_suppressed(bool disabled, bool suppress_test_builds, gtm_build_channel_t channel) {
  if (disabled) {
    return true;
  }
  if (!suppress_test_builds) {
    return false;
  }
  // UNKNOWN is never suppressed: fail open rather than silently lose production data.
  return channel == GTM_BUILD_CHANNEL_DEBUG || channel == GTM_BUILD_CHANNEL_TESTFLIGHT ||
         channel == GTM_BUILD_CHANNEL_INTERNAL_TEST;
}

namespace {

gtm_build_channel_t to_channel(int raw) {
  switch (raw) {
  case GTM_BUILD_CHANNEL_DEBUG:
  case GTM_BUILD_CHANNEL_TESTFLIGHT:
  case GTM_BUILD_CHANNEL_INTERNAL_TEST:
  case GTM_BUILD_CHANNEL_PRODUCTION:
    return static_cast<gtm_build_channel_t>(raw);
  default:
    return GTM_BUILD_CHANNEL_UNKNOWN;
  }
}

// True when the caller's struct (struct_size bytes) fully contains the field.
#define GTM_EXT_HAS(ext, field)                                                        \
  ((ext)->struct_size >= offsetof(gtm_growth_config_ext_t, field) + sizeof((ext)->field))

// Reads an id without creating/persisting one when it is missing.
void load_id_readonly(gtm_growth* c, const char* name, std::string* out) {
  std::string raw;
  if (read_file(c->storage.path_for(name), &raw, nullptr)) {
    *out = trim(raw);
  }
  if (out->empty()) {
    *out = uuid_v4(); // in memory only
  }
}

bool load_identity(gtm_growth* c, std::string* error) {
  if (c->suppressed) {
    load_id_readonly(c, "anonymous_id", &c->anonymous_id);
    load_id_readonly(c, "device_id", &c->device_id);
  } else {
    if (!c->storage.load_or_create_id("anonymous_id", &c->anonymous_id, error)) {
      return false;
    }
    if (!c->storage.load_or_create_id("device_id", &c->device_id, error)) {
      return false;
    }
  }
  std::string uid;
  if (read_file(c->storage.path_for("user_id"), &uid, nullptr)) {
    c->user_id = trim(uid);
  }
  return true;
}

} // namespace
} // namespace internal
} // namespace gtmeasy

extern "C" {

void gtm_growth_config_init(gtm_growth_config_t* config) {
  if (!config) {
    return;
  }
  *config = gtm_growth_config_t{};
  config->flush_at = 20;
  config->flush_interval_ms = 30000;
  config->max_queue_size = 1000;
  config->request_timeout_ms = 10000;
  config->use_batch_endpoint = 1;
  config->log_level = GTM_LOG_WARN;
}

void gtm_growth_config_ext_init(gtm_growth_config_ext_t* ext) {
  if (!ext) {
    return;
  }
  *ext = gtm_growth_config_ext_t{};
  ext->struct_size = static_cast<uint32_t>(sizeof(gtm_growth_config_ext_t));
  ext->build_channel = GTM_BUILD_CHANNEL_UNKNOWN;
}

gtm_error_t gtm_growth_create(const gtm_growth_config_t* config, gtm_growth_t** out) {
  return gtm_growth_create_ext(config, nullptr, out);
}

gtm_error_t gtm_growth_create_ext(const gtm_growth_config_t* config,
                                  const gtm_growth_config_ext_t* ext,
                                  gtm_growth_t** out) {
  using namespace gtmeasy::internal;
  if (!config || !out) {
    return GTM_ERROR_INVALID_ARGUMENT;
  }
  *out = nullptr;
  if (!config->api_key || config->api_key[0] == '\0' || !config->app || config->app[0] == '\0' ||
      !config->cache_dir || config->cache_dir[0] == '\0') {
    return GTM_ERROR_INVALID_ARGUMENT;
  }

  bool disabled = false;
  bool suppress_test_builds = false;
  gtm_build_channel_t channel = GTM_BUILD_CHANNEL_UNKNOWN;
  if (ext) {
    if (ext->struct_size == 0) {
      return GTM_ERROR_INVALID_ARGUMENT; // caller skipped gtm_growth_config_ext_init
    }
    if (GTM_EXT_HAS(ext, suppress_test_builds)) {
      suppress_test_builds = ext->suppress_test_builds != 0;
    }
    if (GTM_EXT_HAS(ext, disabled)) {
      disabled = ext->disabled != 0;
    }
    if (GTM_EXT_HAS(ext, build_channel)) {
      channel = to_channel(ext->build_channel);
    }
  }

  auto* client = new (std::nothrow) gtm_growth(config->cache_dir);
  if (!client) {
    return GTM_ERROR_UNKNOWN;
  }
  client->api_key = config->api_key;
  client->app = config->app;
  client->environment =
      (config->environment && config->environment[0] != '\0') ? config->environment : "production";
  client->base_url = (config->base_url && config->base_url[0] != '\0')
                         ? config->base_url
                         : "https://www.gtmeasy.com/api/v1/growth";
  client->cache_dir = config->cache_dir;
  if (config->app_version) {
    client->app_version = config->app_version;
  }
  if (config->build_number) {
    client->build_number = config->build_number;
  }
  client->flush_at = config->flush_at > 0 ? config->flush_at : 20;
  client->flush_interval_ms = config->flush_interval_ms > 0 ? config->flush_interval_ms : 30000;
  client->max_queue_size = config->max_queue_size > 0 ? config->max_queue_size : 1000;
  client->request_timeout_ms = config->request_timeout_ms > 0 ? config->request_timeout_ms : 10000;
  client->use_batch = config->use_batch_endpoint != 0;
  client->log_level = config->log_level;
  client->log_fn = config->log_fn;
  client->log_user_data = config->log_user_data;
  client->queue.set_max_queue_size(client->max_queue_size);
  client->build_channel = channel;
  client->suppressed = compute_suppressed(disabled, suppress_test_builds, channel);

  std::string err;
  if (client->suppressed) {
    // No durable writes: don't create cache_dir, persist ids, or touch the queue.
    client->clicks.set_persist(false);
    load_identity(client, &err);
    client->clicks.load();
    client->device = snapshot_context();
    client_log(client,
               GTM_LOG_INFO,
               std::string("GTM Easy: suppressing events (") +
                   (disabled ? "disabled, " : "") + "channel=" + build_channel_name(channel) + ")");
    *out = client;
    return GTM_OK;
  }

  if (!client->storage.ensure_dir(&err)) {
    delete client;
    return GTM_ERROR_STORAGE;
  }
  if (!load_identity(client, &err)) {
    delete client;
    return GTM_ERROR_STORAGE;
  }
  client->clicks.load();
  client->device = snapshot_context();
  client->transport = create_transport();

  try {
    client->worker = std::thread([client] { worker_main(client); });
    client->worker_started = true;
  } catch (...) {
    delete client;
    return GTM_ERROR_UNKNOWN;
  }

  *out = client;
  return GTM_OK;
}

void gtm_growth_destroy(gtm_growth_t* client) {
  if (!client) {
    return;
  }
  if (!client->suppressed) {
    gtmeasy::internal::run_flush_cycle(client, 2000);
  }
  client->stop = true;
  client->flush_requested = true;
  client->queue.notify();
  client->cv.notify_all();
  if (client->worker.joinable()) {
    client->worker.join();
  }
  delete client;
}

int gtm_growth_is_suppressed(gtm_growth_t* client) {
  return (client && client->suppressed) ? 1 : 0;
}

gtm_build_channel_t gtm_growth_build_channel(gtm_growth_t* client) {
  return client ? client->build_channel : GTM_BUILD_CHANNEL_UNKNOWN;
}

} // extern "C"
