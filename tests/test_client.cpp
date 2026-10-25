#include "client.hpp"
#include "util.hpp"

#include <gtmeasy/growth.h>
#include <gtmeasy/events.h>

#include <doctest/doctest.h>
#include <httplib.h>

#include "harness.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>


TEST_CASE("batch happy path posts once with api key and drains the queue") {
  Harness h;
  int batch_count = 0;
  h.batch_fn = [&](const httplib::Request& req) {
    ++batch_count;
    auto j = nlohmann::json::parse(req.body);
    httplib::Response res;
    res.status = 200;
    const int n = static_cast<int>(j["events"].size());
    nlohmann::json out;
    out["accepted"] = n;
    out["rejected"] = 0;
    out["results"] = nlohmann::json::array();
    for (int i = 0; i < n; ++i) {
      out["results"].push_back({{"status", 201}});
    }
    res.set_content(out.dump(), "application/json");
    return res;
  };
  h.start_client();
  for (int i = 0; i < 5; ++i) {
    REQUIRE(h.client->track("feature.used", {{"i", i}}) == GTM_OK);
  }
  REQUIRE(h.client->flush(5000) == GTM_OK);
  CHECK(batch_count == 1);
  auto hits = h.snapshot();
  REQUIRE_FALSE(hits.empty());
  CHECK(hits[0].path == "/events/batch");
  CHECK(hits[0].key == "test-write-key");
  CHECK(h.queue_empty());
}

TEST_CASE("404 on batch falls back to single /events posts") {
  Harness h;
  h.batch_fn = [](const httplib::Request&) {
    httplib::Response res;
    res.status = 404;
    res.set_content(R"({"error":"not found"})", "application/json");
    return res;
  };
  h.start_client();
  REQUIRE(h.client->track("page.viewed") == GTM_OK);
  REQUIRE(h.client->track("button.clicked") == GTM_OK);
  REQUIRE(h.client->flush(5000) == GTM_OK);
  int singles = 0;
  int batches = 0;
  for (const auto& hit : h.snapshot()) {
    if (hit.path == "/events") {
      ++singles;
    }
    if (hit.path == "/events/batch") {
      ++batches;
    }
  }
  CHECK(batches >= 1);
  CHECK(singles == 2);
  CHECK(h.queue_empty());
}

TEST_CASE("429 with retry-after 1 pauses then succeeds") {
  Harness h;
  std::atomic<int> n{0};
  h.batch_fn = [&](const httplib::Request&) {
    httplib::Response res;
    if (n.fetch_add(1) == 0) {
      res.status = 429;
      res.set_header("Retry-After", "1");
      res.set_content("{}", "application/json");
      return res;
    }
    res.status = 200;
    res.set_content(R"({"accepted":1,"rejected":0,"results":[{"status":201}]})", "application/json");
    return res;
  };
  h.start_client();
  REQUIRE(h.client->track("feature.used") == GTM_OK);
  REQUIRE(h.client->flush(8000) == GTM_OK);
  auto hits = h.snapshot();
  REQUIRE(hits.size() >= 2);
  const auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(hits[1].at - hits[0].at);
  CHECK(gap.count() >= 1000);
  CHECK(h.queue_empty());
}

TEST_CASE("401 stops the worker and leaves events on disk") {
  Harness h;
  h.batch_fn = [](const httplib::Request&) {
    httplib::Response res;
    res.status = 401;
    res.set_content(R"({"error":"unauthorized"})", "application/json");
    return res;
  };
  h.start_client();
  REQUIRE(h.client->track("feature.used") == GTM_OK);
  const auto err = h.client->flush(3000);
  CHECK(err == GTM_ERROR_INVALID_CREDENTIALS);
  CHECK_FALSE(h.queue_empty());
  const auto after_first = h.snapshot().size();
  REQUIRE(h.client->track("page.viewed") == GTM_OK);
  (void)h.client->flush(1000);
  CHECK(h.snapshot().size() == after_first);
  CHECK_FALSE(h.queue_empty());
}

TEST_CASE("5xx is retried with backoff then succeeds") {
  Harness h;
  std::atomic<int> n{0};
  h.batch_fn = [&](const httplib::Request&) {
    httplib::Response res;
    if (n.fetch_add(1) == 0) {
      res.status = 503;
      res.set_content("{}", "application/json");
      return res;
    }
    res.status = 200;
    res.set_content(R"({"accepted":1,"rejected":0,"results":[{"status":201}]})", "application/json");
    return res;
  };
  h.start_client();
  REQUIRE(h.client->track("error.occurred") == GTM_OK);
  REQUIRE(h.client->flush(10000) == GTM_OK);
  CHECK(h.snapshot().size() >= 2);
  CHECK(h.queue_empty());
}

TEST_CASE("identify goes to /users before events") {
  Harness h;
  h.start_client();
  REQUIRE(h.client->track("feature.used") == GTM_OK);
  REQUIRE(h.client->identify("user-42", {{"plan", "free"}}) == GTM_OK);
  REQUIRE(h.client->flush(5000) == GTM_OK);
  auto hits = h.snapshot();
  REQUIRE(hits.size() >= 2);
  CHECK(hits[0].path == "/users");
  bool saw_event = false;
  for (size_t i = 1; i < hits.size(); ++i) {
    if (hits[i].path == "/events/batch" || hits[i].path == "/events") {
      saw_event = true;
    }
  }
  CHECK(saw_event);
}

TEST_CASE("track_first_open emits first_open then updated after version change") {
  Harness h;
  h.start_client([](gtmeasy::Config& c) { c.appVersion = "1.0.0"; });
  REQUIRE(h.client->trackFirstOpen() == GTM_OK);
  REQUIRE(h.client->flush(5000) == GTM_OK);
  auto body = h.snapshot().back().body;
  auto j = nlohmann::json::parse(body);
  REQUIRE(j.contains("events"));
  CHECK(j["events"][0]["eventName"] == "app.first_open");
  CHECK(j["events"][1]["eventName"] == "app.opened");
  h.client.reset();

  h.hits.clear();
  h.start_client([](gtmeasy::Config& c) { c.appVersion = "1.1.0"; });
  REQUIRE(h.client->trackFirstOpen() == GTM_OK);
  REQUIRE(h.client->flush(5000) == GTM_OK);
  auto body2 = h.snapshot().back().body;
  auto j2 = nlohmann::json::parse(body2);
  CHECK(j2["events"][0]["eventName"] == "app.updated");
  CHECK(j2["events"][1]["eventName"] == "app.opened");
}

TEST_CASE("destroy flushes pending events") {
  Harness h;
  h.start_client();
  REQUIRE(h.client->track("feature.used", {{"from", "destroy"}}) == GTM_OK);
  h.client.reset();
  CHECK_FALSE(h.snapshot().empty());
}
