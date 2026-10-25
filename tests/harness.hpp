// Shared mock-server harness for client tests.
#pragma once

#include "util.hpp"

#include <gtmeasy/growth.h>

#include <doctest/doctest.h>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

struct Recorded {
  std::string path;
  std::string body;
  std::string key;
  std::chrono::steady_clock::time_point at;
};

struct Harness {
  httplib::Server svr;
  std::thread th;
  int port = 0;
  std::mutex mu;
  std::vector<Recorded> hits;
  fs::path dir;
  std::unique_ptr<gtmeasy::GrowthAnalytics> client;

  std::function<httplib::Response(const httplib::Request&)> batch_fn;
  std::function<httplib::Response(const httplib::Request&)> events_fn;
  std::function<httplib::Response(const httplib::Request&)> users_fn;

  Harness() {
    gtmeasy::internal::reset_batch_supported_for_tests();
    dir = fs::temp_directory_path() / ("gtm-client-" + gtmeasy::internal::uuid_v4());
    fs::create_directories(dir);

    auto record = [this](const httplib::Request& req) {
      Recorded r;
      r.path = req.path;
      r.body = req.body;
      r.key = req.get_header_value("x-gtm-growth-key");
      r.at = std::chrono::steady_clock::now();
      std::lock_guard<std::mutex> lock(mu);
      hits.push_back(std::move(r));
    };

    svr.Post("/events/batch", [this, record](const httplib::Request& req, httplib::Response& res) {
      record(req);
      if (batch_fn) {
        res = batch_fn(req);
        return;
      }
      res.status = 200;
      res.set_content(R"({"accepted":1,"rejected":0,"results":[{"status":201}]})", "application/json");
    });
    svr.Post("/events", [this, record](const httplib::Request& req, httplib::Response& res) {
      record(req);
      if (events_fn) {
        res = events_fn(req);
        return;
      }
      res.status = 201;
      res.set_content("{}", "application/json");
    });
    svr.Post("/users", [this, record](const httplib::Request& req, httplib::Response& res) {
      record(req);
      if (users_fn) {
        res = users_fn(req);
        return;
      }
      res.status = 201;
      res.set_content("{}", "application/json");
    });

    port = svr.bind_to_any_port("127.0.0.1");
    th = std::thread([this] { svr.listen_after_bind(); });
    for (int i = 0; i < 50 && !svr.is_running(); ++i) {
      std::this_thread::sleep_for(20ms);
    }
  }

  ~Harness() {
    client.reset();
    svr.stop();
    if (th.joinable()) {
      th.join();
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  std::string base() const { return "http://127.0.0.1:" + std::to_string(port); }

  void start_client(const std::function<void(gtmeasy::Config&)>& tweak = {}) {
    gtmeasy::Config cfg;
    cfg.apiKey = "test-write-key";
    cfg.app = "test-app";
    cfg.cacheDir = dir.string();
    cfg.baseUrl = base();
    cfg.flushAt = 1000;
    cfg.flushIntervalMs = 60000;
    cfg.requestTimeoutMs = 3000;
    if (tweak) {
      tweak(cfg);
    }
    client = gtmeasy::GrowthAnalytics::create(cfg);
    REQUIRE(client != nullptr);
  }

  std::vector<Recorded> snapshot() {
    std::lock_guard<std::mutex> lock(mu);
    return hits;
  }

  bool queue_empty() const {
    const auto q = dir / "queue.jsonl";
    if (!fs::exists(q) || fs::file_size(q) == 0) {
      return true;
    }
    std::ifstream in(q);
    std::string line;
    while (std::getline(in, line)) {
      if (!gtmeasy::internal::trim(line).empty()) {
        return false;
      }
    }
    return true;
  }
};

} // namespace
