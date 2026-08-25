#include "client.hpp"

#include "util.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <thread>
#include <vector>

namespace gtmeasy {
namespace internal {
namespace {

std::atomic<bool> g_batch_supported{true};

using clock = std::chrono::steady_clock;

bool deadline_reached(clock::time_point deadline) {
  return deadline.time_since_epoch().count() != 0 && clock::now() >= deadline;
}

void sleep_interruptible(gtm_growth* c, std::chrono::milliseconds total, clock::time_point deadline) {
  auto remaining = total;
  while (remaining.count() > 0 && !c->stop.load()) {
    if (deadline_reached(deadline)) {
      break;
    }
    auto slice = remaining < std::chrono::milliseconds(50) ? remaining : std::chrono::milliseconds(50);
    std::this_thread::sleep_for(slice);
    remaining -= slice;
  }
}

std::map<std::string, std::string> headers_for(gtm_growth* c) {
  return {{"Content-Type", "application/json"}, {"x-gtm-growth-key", c->api_key}};
}

int request_timeout(gtm_growth* c, clock::time_point deadline) {
  int ms = c->request_timeout_ms;
  if (deadline.time_since_epoch().count() != 0) {
    const auto left =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()).count();
    if (left <= 0) {
      return 1;
    }
    if (left < ms) {
      ms = static_cast<int>(left);
    }
  }
  return ms < 1 ? 1 : ms;
}

enum class SendResult { Ok, Drop, RetryLater, AuthFailed };

SendResult classify(const HttpResponse& resp) {
  if (!resp.error.empty() && resp.status == 0) {
    return SendResult::RetryLater;
  }
  if (resp.status == 401) {
    return SendResult::AuthFailed;
  }
  if (resp.status == 429) {
    return SendResult::RetryLater;
  }
  if (resp.status == 400) {
    return SendResult::Drop;
  }
  if (resp.status >= 200 && resp.status < 300) {
    return SendResult::Ok;
  }
  if (resp.status >= 500 || resp.status == 0) {
    return SendResult::RetryLater;
  }
  if (resp.status == 404) {
    return SendResult::RetryLater;
  }
  return SendResult::Drop;
}

HttpResponse post_path(gtm_growth* c, const std::string& path, const std::string& body,
                       clock::time_point deadline) {
  const std::string url = join_url(c->base_url, path);
  return c->transport->post(url, headers_for(c), body, request_timeout(c, deadline));
}

SendResult send_with_retries(gtm_growth* c,
                             const std::string& path,
                             const std::string& body,
                             clock::time_point deadline,
                             HttpResponse* last) {
  int net_attempts = 0;
  while (!c->stop.load() && !deadline_reached(deadline)) {
    HttpResponse resp = post_path(c, path, body, deadline);
    if (last) {
      *last = resp;
    }
    if (resp.status == 429) {
      const int wait_s = resp.retry_after_seconds > 0 ? resp.retry_after_seconds : 30;
      client_log(c, GTM_LOG_WARN, "rate limited; sleeping " + std::to_string(wait_s) + "s");
      sleep_interruptible(c, std::chrono::seconds(wait_s), deadline);
      continue;
    }
    const SendResult r = classify(resp);
    if (r == SendResult::Ok || r == SendResult::Drop || r == SendResult::AuthFailed) {
      return r;
    }
    if (resp.status == 404 && path.find("batch") != std::string::npos) {
      return r;
    }
    if (net_attempts >= 6) {
      return SendResult::RetryLater;
    }
    ++net_attempts;
    const double sec = backoff_seconds(net_attempts - 1);
    client_log(c, GTM_LOG_WARN, "retrying " + path + " after " + std::to_string(sec) + "s");
    sleep_interruptible(
        c, std::chrono::milliseconds(static_cast<int>(sec * 1000.0)), deadline);
  }
  return SendResult::RetryLater;
}

void mark_auth_failed(gtm_growth* c) {
  if (!c->auth_failed.exchange(true)) {
    client_log(c, GTM_LOG_ERROR, "invalid credentials; worker stopped");
  }
}

void apply_batch_results(const HttpResponse& resp,
                         const std::vector<int>& event_indices,
                         std::vector<char>* keep) {
  auto j = nlohmann::json::parse(resp.body, nullptr, false);
  if (j.is_discarded() || !j.contains("results") || !j["results"].is_array()) {
    if (resp.status >= 200 && resp.status < 300) {
      for (int idx : event_indices) {
        (*keep)[static_cast<size_t>(idx)] = 0;
      }
    }
    return;
  }
  const auto& results = j["results"];
  for (size_t i = 0; i < event_indices.size(); ++i) {
    int status = 200;
    if (i < results.size()) {
      if (results[i].is_object()) {
        status = results[i].value("status", 200);
      } else if (results[i].is_number()) {
        status = results[i].get<int>();
      }
    }
    const int qi = event_indices[i];
    if (status == 400 || (status >= 200 && status < 300)) {
      (*keep)[static_cast<size_t>(qi)] = 0;
    }
  }
}

bool send_events_singles(gtm_growth* c,
                         const std::vector<QueueItem>& items,
                         const std::vector<int>& event_indices,
                         std::vector<char>* keep,
                         clock::time_point deadline) {
  for (int qi : event_indices) {
    if (c->auth_failed || c->stop || deadline_reached(deadline)) {
      return false;
    }
    HttpResponse last;
    const SendResult r =
        send_with_retries(c, "/events", items[static_cast<size_t>(qi)].body.dump(), deadline, &last);
    if (r == SendResult::AuthFailed) {
      mark_auth_failed(c);
      return false;
    }
    if (r == SendResult::RetryLater) {
      return false;
    }
    (*keep)[static_cast<size_t>(qi)] = 0;
  }
  return true;
}

bool send_events(gtm_growth* c,
                 const std::vector<QueueItem>& items,
                 const std::vector<int>& event_indices,
                 std::vector<char>* keep,
                 clock::time_point deadline) {
  if (event_indices.empty()) {
    return true;
  }
  const bool can_batch = c->use_batch && g_batch_supported.load();
  if (can_batch) {
    nlohmann::json payload;
    payload["events"] = nlohmann::json::array();
    for (int qi : event_indices) {
      payload["events"].push_back(items[static_cast<size_t>(qi)].body);
    }
    HttpResponse last;
    const SendResult r =
        send_with_retries(c, "/events/batch", payload.dump(), deadline, &last);
    if (last.status == 404) {
      g_batch_supported = false;
      client_log(c, GTM_LOG_INFO, "batch endpoint missing; falling back to /events");
      return send_events_singles(c, items, event_indices, keep, deadline);
    }
    if (r == SendResult::AuthFailed) {
      mark_auth_failed(c);
      return false;
    }
    if (r == SendResult::RetryLater) {
      return false;
    }
    if (r == SendResult::Drop) {
      apply_batch_results(last, event_indices, keep);
      if (last.status == 400 && last.body.empty()) {
        for (int qi : event_indices) {
          (*keep)[static_cast<size_t>(qi)] = 0;
        }
      }
      return true;
    }
    apply_batch_results(last, event_indices, keep);
    return true;
  }
  return send_events_singles(c, items, event_indices, keep, deadline);
}

} // namespace

void reset_batch_supported_for_tests() { g_batch_supported = true; }

void run_flush_cycle(gtm_growth* c, int timeout_ms_budget) {
  if (!c || c->auth_failed.load()) {
    return;
  }
  std::lock_guard<std::mutex> flush_lock(c->flush_mu);
  if (c->auth_failed.load()) {
    return;
  }

  clock::time_point deadline{};
  if (timeout_ms_budget > 0) {
    deadline = clock::now() + std::chrono::milliseconds(timeout_ms_budget);
  }

  auto items = c->queue.snapshot();
  if (items.empty()) {
    return;
  }
  const int prefix = (std::min)(100, static_cast<int>(items.size()));
  std::vector<char> keep(static_cast<size_t>(prefix), 1);

  for (int i = 0; i < prefix; ++i) {
    if (items[static_cast<size_t>(i)].kind != "user") {
      continue;
    }
    if (c->auth_failed || c->stop || deadline_reached(deadline)) {
      std::string err;
      c->queue.compact_keep(keep, prefix, &err);
      return;
    }
    HttpResponse last;
    const SendResult r = send_with_retries(
        c, "/users", items[static_cast<size_t>(i)].body.dump(), deadline, &last);
    if (r == SendResult::AuthFailed) {
      mark_auth_failed(c);
      std::string err;
      c->queue.compact_keep(keep, prefix, &err);
      return;
    }
    if (r == SendResult::RetryLater) {
      std::string err;
      c->queue.compact_keep(keep, prefix, &err);
      return;
    }
    keep[static_cast<size_t>(i)] = 0;
  }

  std::vector<int> event_indices;
  for (int i = 0; i < prefix; ++i) {
    if (items[static_cast<size_t>(i)].kind == "event" && keep[static_cast<size_t>(i)]) {
      event_indices.push_back(i);
    }
  }
  send_events(c, items, event_indices, &keep, deadline);

  if (c->queue.dropped() > 0) {
    client_log(c,
               GTM_LOG_WARN,
               "dropped " + std::to_string(c->queue.dropped()) + " oldest queued events");
  }

  std::string err;
  c->queue.compact_keep(keep, prefix, &err);
}

void worker_main(gtm_growth* c) {
  while (!c->stop.load()) {
    c->queue.wait_for_work(c->flush_interval_ms, c->flush_at, [&] {
      return c->stop.load() || c->flush_requested.load();
    });
    if (c->stop.load()) {
      break;
    }
    if (c->auth_failed.load()) {
      continue;
    }
    c->flush_requested = false;
    if (c->queue.size() == 0) {
      continue;
    }
    run_flush_cycle(c, 0);
  }
}

} // namespace internal
} // namespace gtmeasy
