#include "client.hpp"

#include "gtmeasy/events.h"
#include "gtmeasy/version.h"
#include "models.hpp"
#include "util.hpp"

#include <chrono>
#include <cstdio>
#include <new>

namespace gtmeasy {
namespace internal {

void client_log(gtm_growth* c, gtm_log_level_t level, const std::string& message) {
  if (!c || level < c->log_level || c->log_level == GTM_LOG_NONE) {
    return;
  }
  std::string msg = "[gtmeasy] " + message;
  if (!c->api_key.empty()) {
    const auto pos = msg.find(c->api_key);
    if (pos != std::string::npos) {
      msg.replace(pos, c->api_key.size(), "***");
    }
  }
  if (c->log_fn) {
    c->log_fn(level, msg.c_str(), c->log_user_data);
  } else {
    std::fprintf(stderr, "%s\n", msg.c_str());
  }
}

nlohmann::json parse_object_json(const char* json, gtm_error_t* err) {
  if (json == nullptr || json[0] == '\0') {
    return nlohmann::json::object();
  }
  auto parsed = nlohmann::json::parse(json, nullptr, false);
  if (parsed.is_discarded() || !parsed.is_object()) {
    if (err) {
      *err = GTM_ERROR_INVALID_PAYLOAD;
    }
    return {};
  }
  return parsed;
}

nlohmann::json build_ctx(gtm_growth* c) {
  nlohmann::json ctx = context_to_json(c->device);
  c->clicks.merge_into(ctx);
  return ctx;
}

nlohmann::json build_event(gtm_growth* c,
                           const std::string& event_name,
                           nlohmann::json properties,
                           bool has_metric,
                           double metric_value,
                           const char* metric_label) {
  if (!properties.is_object()) {
    properties = nlohmann::json::object();
  }
  nlohmann::json ctx = build_ctx(c);
  if (properties.contains("_ctx") && properties["_ctx"].is_object()) {
    auto merged = properties["_ctx"];
    for (auto it = ctx.begin(); it != ctx.end(); ++it) {
      merged[it.key()] = it.value();
    }
    properties["_ctx"] = merged;
  } else {
    properties["_ctx"] = ctx;
  }

  nlohmann::json body = envelope_base(c->app,
                                      c->environment,
                                      c->device.platform,
                                      c->user_id,
                                      c->anonymous_id,
                                      c->device_id,
                                      c->app_version,
                                      c->build_number,
                                      c->device.locale,
                                      c->device.timezone);
  body["eventId"] = uuid_v4();
  body["eventName"] = event_name;
  body["occurredAt"] = rfc3339_now();
  body["properties"] = properties;
  if (has_metric) {
    body["metricValue"] = metric_value;
    if (metric_label && metric_label[0] != '\0') {
      body["metricLabel"] = metric_label;
    }
  }
  return body;
}

nlohmann::json build_identify(gtm_growth* c,
                              const char* user_id,
                              const char* username,
                              const char* email,
                              nlohmann::json traits) {
  if (!traits.is_object()) {
    traits = nlohmann::json::object();
  }
  nlohmann::json ctx = build_ctx(c);
  if (traits.contains("_ctx") && traits["_ctx"].is_object()) {
    auto merged = traits["_ctx"];
    for (auto it = ctx.begin(); it != ctx.end(); ++it) {
      merged[it.key()] = it.value();
    }
    traits["_ctx"] = merged;
  } else {
    traits["_ctx"] = ctx;
  }

  nlohmann::json body = envelope_base(c->app,
                                      c->environment,
                                      c->device.platform,
                                      user_id ? std::string(user_id) : c->user_id,
                                      c->anonymous_id,
                                      c->device_id,
                                      c->app_version,
                                      c->build_number,
                                      c->device.locale,
                                      c->device.timezone);
  if (username && username[0] != '\0') {
    body["username"] = username;
  }
  if (email && email[0] != '\0') {
    body["email"] = email;
  }
  body["traits"] = traits;
  return body;
}

} // namespace internal
} // namespace gtmeasy

extern "C" {

gtm_error_t gtm_growth_identify(gtm_growth_t* client,
                                const char* user_id,
                                const char* username,
                                const char* email,
                                const char* traits_json) {
  if (!client) {
    return GTM_ERROR_NOT_INITIALIZED;
  }
  if (!user_id || user_id[0] == '\0') {
    return GTM_ERROR_INVALID_ARGUMENT;
  }
  gtm_error_t err = GTM_OK;
  auto traits = gtmeasy::internal::parse_object_json(traits_json, &err);
  if (err != GTM_OK) {
    return err;
  }
  client->user_id = user_id;
  if (client->suppressed) {
    return GTM_OK; // memory only: no persistence, no enqueue
  }
  std::string werr;
  client->storage.write_id("user_id", user_id, &werr);
  auto body = gtmeasy::internal::build_identify(client, user_id, username, email, traits);
  if (!client->queue.enqueue("user", body, &werr)) {
    return GTM_ERROR_STORAGE;
  }
  if (client->queue.size() >= client->flush_at) {
    client->flush_requested = true;
    client->queue.notify();
  }
  return GTM_OK;
}

gtm_error_t gtm_growth_track(gtm_growth_t* client,
                             const char* event_name,
                             const char* properties_json) {
  if (!client) {
    return GTM_ERROR_NOT_INITIALIZED;
  }
  if (!event_name || event_name[0] == '\0') {
    return GTM_ERROR_INVALID_ARGUMENT;
  }
  gtm_error_t err = GTM_OK;
  auto props = gtmeasy::internal::parse_object_json(properties_json, &err);
  if (err != GTM_OK) {
    return err;
  }
  if (client->suppressed) {
    return GTM_OK;
  }
  auto body = gtmeasy::internal::build_event(client, event_name, props, false, 0, nullptr);
  std::string werr;
  if (!client->queue.enqueue("event", body, &werr)) {
    return GTM_ERROR_STORAGE;
  }
  if (client->queue.size() >= client->flush_at) {
    client->flush_requested = true;
    client->queue.notify();
  }
  return GTM_OK;
}

gtm_error_t gtm_growth_track_metric(gtm_growth_t* client,
                                    const char* event_name,
                                    const char* properties_json,
                                    double metric_value,
                                    const char* metric_label) {
  if (!client) {
    return GTM_ERROR_NOT_INITIALIZED;
  }
  if (!event_name || event_name[0] == '\0') {
    return GTM_ERROR_INVALID_ARGUMENT;
  }
  gtm_error_t err = GTM_OK;
  auto props = gtmeasy::internal::parse_object_json(properties_json, &err);
  if (err != GTM_OK) {
    return err;
  }
  if (client->suppressed) {
    return GTM_OK;
  }
  auto body =
      gtmeasy::internal::build_event(client, event_name, props, true, metric_value, metric_label);
  std::string werr;
  if (!client->queue.enqueue("event", body, &werr)) {
    return GTM_ERROR_STORAGE;
  }
  if (client->queue.size() >= client->flush_at) {
    client->flush_requested = true;
    client->queue.notify();
  }
  return GTM_OK;
}

gtm_error_t gtm_growth_track_first_open(gtm_growth_t* client) {
  if (!client) {
    return GTM_ERROR_NOT_INITIALIZED;
  }
  if (client->suppressed) {
    return GTM_OK; // no events and no last_version write
  }
  nlohmann::json last;
  const bool had = client->storage.read_json("last_version", &last, nullptr);
  const std::string prev_ver = had ? last.value("app_version", "") : "";
  const std::string prev_build = had ? last.value("build_number", "") : "";
  const bool version_changed =
      prev_ver != client->app_version || prev_build != client->build_number;

  gtm_error_t err = GTM_OK;
  if (!had) {
    err = gtm_growth_track(client, gtmeasy::events::app_first_open, nullptr);
    if (err != GTM_OK) {
      return err;
    }
  } else if (version_changed) {
    nlohmann::json props = nlohmann::json::object();
    if (!prev_ver.empty()) {
      props["previous_version"] = prev_ver;
    }
    if (!prev_build.empty()) {
      props["previous_build"] = prev_build;
    }
    const std::string dumped = props.dump();
    err = gtm_growth_track(client, gtmeasy::events::app_updated, dumped.c_str());
    if (err != GTM_OK) {
      return err;
    }
  }
  err = gtm_growth_track(client, gtmeasy::events::app_opened, nullptr);
  if (err != GTM_OK) {
    return err;
  }
  nlohmann::json cur = {{"app_version", client->app_version}, {"build_number", client->build_number}};
  std::string werr;
  client->storage.write_json("last_version", cur, &werr);
  return GTM_OK;
}

gtm_error_t gtm_growth_record_click_id(gtm_growth_t* client,
                                       const char* provider,
                                       const char* value) {
  if (!client) {
    return GTM_ERROR_NOT_INITIALIZED;
  }
  if (!provider || !value) {
    return GTM_ERROR_INVALID_ARGUMENT;
  }
  std::string err;
  if (!client->clicks.record(provider, value, &err)) {
    return GTM_ERROR_INVALID_ARGUMENT;
  }
  return GTM_OK;
}

int gtm_growth_capture_click_ids(gtm_growth_t* client, const char* url) {
  if (!client || !url) {
    return 0;
  }
  return client->clicks.capture_url(url);
}

gtm_error_t gtm_growth_flush(gtm_growth_t* client, int timeout_ms) {
  if (!client) {
    return GTM_ERROR_NOT_INITIALIZED;
  }
  if (client->suppressed) {
    return GTM_OK; // never send, never touch the persisted queue
  }
  client->flush_requested = true;
  client->queue.notify();
  client->cv.notify_all();
  if (timeout_ms <= 0) {
    return GTM_OK;
  }
  gtmeasy::internal::run_flush_cycle(client, timeout_ms);
  const bool drained = client->queue.wait_until(1, [&] {
    return client->queue.size_unlocked() == 0 || client->auth_failed.load() || client->stop.load();
  });
  if (client->auth_failed) {
    return GTM_ERROR_INVALID_CREDENTIALS;
  }
  if (!drained && client->queue.size() > 0) {
    return GTM_ERROR_TIMEOUT;
  }
  return GTM_OK;
}

gtm_error_t gtm_growth_reset(gtm_growth_t* client) {
  if (!client) {
    return GTM_ERROR_NOT_INITIALIZED;
  }
  std::string err;
  client->user_id.clear();
  client->anonymous_id = gtmeasy::internal::uuid_v4();
  if (client->suppressed) {
    return GTM_OK; // memory only
  }
  client->storage.delete_id("user_id", &err);
  if (!client->storage.write_id("anonymous_id", client->anonymous_id, &err)) {
    return GTM_ERROR_STORAGE;
  }
  return GTM_OK;
}

const char* gtm_growth_anonymous_id(gtm_growth_t* client) {
  if (!client) {
    return nullptr;
  }
  return client->anonymous_id.c_str();
}

const char* gtm_growth_user_id(gtm_growth_t* client) {
  if (!client) {
    return nullptr;
  }
  if (client->user_id.empty()) {
    return nullptr;
  }
  return client->user_id.c_str();
}

const char* gtm_growth_version(void) { return GTMEASY_GROWTH_VERSION; }

} // extern "C"
