#include "queue.hpp"
#include "storage.hpp"
#include "util.hpp"

#include <gtmeasy/growth.h>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path make_temp_dir() {
  const fs::path p = fs::temp_directory_path() / ("gtm-storage-" + gtmeasy::internal::uuid_v4());
  fs::create_directories(p);
  return p;
}

} // namespace

TEST_CASE("Storage append/read/compact JSONL and atomic replace") {
  const auto dir = make_temp_dir();
  gtmeasy::internal::Storage storage(dir.string());
  REQUIRE(storage.ensure_dir(nullptr));

  nlohmann::json a = {{"n", 1}};
  nlohmann::json line;
  line["kind"] = "event";
  line["body"] = a;
  REQUIRE(storage.append_queue_line(line.dump(), nullptr));
  nlohmann::json b = {{"n", 2}};
  line["body"] = b;
  REQUIRE(storage.append_queue_line(line.dump(), nullptr));

  std::vector<gtmeasy::internal::QueueItem> items;
  REQUIRE(storage.read_queue(&items, nullptr));
  REQUIRE(items.size() == 2);
  CHECK(items[0].body["n"] == 1);

  items.erase(items.begin());
  REQUIRE(storage.rewrite_queue(items, nullptr));
  items.clear();
  REQUIRE(storage.read_queue(&items, nullptr));
  REQUIRE(items.size() == 1);
  CHECK(items[0].body["n"] == 2);

  REQUIRE(gtmeasy::internal::atomic_write_file((dir / "x.txt").string(), "hello", nullptr));
  std::string out;
  REQUIRE(gtmeasy::internal::read_file((dir / "x.txt").string(), &out, nullptr));
  CHECK(out == "hello");

  fs::remove_all(dir);
}

TEST_CASE("Storage max_queue_size drops oldest") {
  const auto dir = make_temp_dir();
  gtmeasy::internal::Storage storage(dir.string());
  REQUIRE(storage.ensure_dir(nullptr));
  gtmeasy::internal::Queue queue(&storage, 3);
  for (int i = 0; i < 5; ++i) {
    nlohmann::json body = {{"i", i}};
    REQUIRE(queue.enqueue("event", body, nullptr));
  }
  CHECK(queue.size() == 3);
  CHECK(queue.dropped() >= 2);
  auto items = queue.snapshot();
  REQUIRE(items.size() == 3);
  CHECK(items.front().body["i"] == 2);
  CHECK(items.back().body["i"] == 4);
  fs::remove_all(dir);
}

TEST_CASE("anonymous/device id persistence and reset() semantics") {
  const auto dir = make_temp_dir();
  gtmeasy::Config cfg;
  cfg.apiKey = "test-key";
  cfg.app = "test-app";
  cfg.cacheDir = dir.string();
  cfg.baseUrl = "http://127.0.0.1:1";
  cfg.flushIntervalMs = 60000;
  cfg.flushAt = 1000;

  auto a = gtmeasy::GrowthAnalytics::create(cfg);
  REQUIRE(a != nullptr);
  const std::string anon1 = a->anonymousId();
  CHECK_FALSE(anon1.empty());
  CHECK_FALSE(a->userId().has_value());

  REQUIRE(a->identify("user-1") == GTM_OK);
  CHECK(a->userId() == std::optional<std::string>("user-1"));
  a.reset();

  auto b = gtmeasy::GrowthAnalytics::create(cfg);
  REQUIRE(b != nullptr);
  CHECK(b->anonymousId() == anon1);
  CHECK(b->userId() == std::optional<std::string>("user-1"));

  REQUIRE(b->reset() == GTM_OK);
  CHECK(b->anonymousId() != anon1);
  CHECK_FALSE(b->userId().has_value());
  const std::string anon2 = b->anonymousId();
  b.reset();

  auto c = gtmeasy::GrowthAnalytics::create(cfg);
  REQUIRE(c != nullptr);
  CHECK(c->anonymousId() == anon2);
  CHECK_FALSE(c->userId().has_value());

  std::string device1;
  std::string device2;
  REQUIRE(gtmeasy::internal::read_file((dir / "device_id").string(), &device1, nullptr));
  c.reset();
  REQUIRE(gtmeasy::internal::read_file((dir / "device_id").string(), &device2, nullptr));
  CHECK(gtmeasy::internal::trim(device1) == gtmeasy::internal::trim(device2));

  fs::remove_all(dir);
}
