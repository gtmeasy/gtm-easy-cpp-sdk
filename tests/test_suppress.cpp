#include "client.hpp"

#include "harness.hpp"

#include <gtmeasy/growth_c.h>

#include <cstddef>
#include <cstring>

namespace {

std::string read_all(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_all(const fs::path& p, const std::string& s) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out << s;
}

int count_event(const std::vector<Recorded>& hits, const std::string& name) {
  int n = 0;
  for (const auto& h : hits) {
    auto j = nlohmann::json::parse(h.body, nullptr, false);
    if (j.is_discarded()) {
      continue;
    }
    if (j.contains("events")) {
      for (const auto& e : j["events"]) {
        n += e.value("eventName", "") == name ? 1 : 0;
      }
    } else if (j.value("eventName", "") == name) {
      ++n;
    }
  }
  return n;
}

void suppress_debug(gtmeasy::Config& c) {
  c.suppressTestBuilds = true;
  c.buildChannel = gtmeasy::BuildChannel::Debug;
}

} // namespace

TEST_CASE("suppression rule: disabled || (suppress_test_builds && test channel); unknown never") {
  using gtmeasy::internal::compute_suppressed;
  CHECK(compute_suppressed(true, false, GTM_BUILD_CHANNEL_PRODUCTION));
  CHECK(compute_suppressed(true, false, GTM_BUILD_CHANNEL_UNKNOWN));
  CHECK(compute_suppressed(false, true, GTM_BUILD_CHANNEL_DEBUG));
  CHECK(compute_suppressed(false, true, GTM_BUILD_CHANNEL_TESTFLIGHT));
  CHECK(compute_suppressed(false, true, GTM_BUILD_CHANNEL_INTERNAL_TEST));
  CHECK_FALSE(compute_suppressed(false, true, GTM_BUILD_CHANNEL_PRODUCTION));
  CHECK_FALSE(compute_suppressed(false, true, GTM_BUILD_CHANNEL_UNKNOWN));
  CHECK_FALSE(compute_suppressed(false, false, GTM_BUILD_CHANNEL_DEBUG));
}

TEST_CASE("suppressed client sends nothing and leaves a prepopulated queue untouched") {
  Harness h;
  const std::string queued =
      R"({"kind":"event","body":{"eventName":"prequeued","eventId":"x","properties":{}}})"
      "\n";
  write_all(h.dir / "queue.jsonl", queued);
  write_all(h.dir / "anonymous_id", "anon-existing\n");

  h.start_client(suppress_debug);
  CHECK(h.client->isSuppressed());
  CHECK(h.client->buildChannel() == gtmeasy::BuildChannel::Debug);
  CHECK(h.client->anonymousId() == "anon-existing");

  CHECK(h.client->track("feature.used", {{"a", 1}}) == GTM_OK);
  CHECK(h.client->trackMetric("timing", 1.5, "ms") == GTM_OK);
  CHECK(h.client->identify("user-1", {{"plan", "pro"}}) == GTM_OK);
  CHECK(h.client->userId() == std::optional<std::string>("user-1"));
  CHECK(h.client->trackFirstOpen() == GTM_OK);
  CHECK(h.client->recordClickId("gclid", "abc") == GTM_OK);
  CHECK(h.client->flush(2000) == GTM_OK);
  h.client.reset(); // destructor: no final flush

  CHECK(h.snapshot().empty());
  CHECK(read_all(h.dir / "queue.jsonl") == queued);
  CHECK(read_all(h.dir / "anonymous_id") == "anon-existing\n");
  CHECK_FALSE(fs::exists(h.dir / "last_version"));
  CHECK_FALSE(fs::exists(h.dir / "user_id"));
  CHECK_FALSE(fs::exists(h.dir / "device_id"));
  CHECK_FALSE(fs::exists(h.dir / "click_ids.json"));
}

TEST_CASE("suppressed client never creates cache_dir; anonymous id is in-memory only") {
  Harness h;
  const auto sub = h.dir / "fresh";
  h.start_client([&](gtmeasy::Config& c) {
    c.cacheDir = sub.string();
    c.disabled = true; // plain disabled, any channel
    c.buildChannel = gtmeasy::BuildChannel::Production;
  });
  CHECK(h.client->isSuppressed());
  const auto anon = h.client->anonymousId();
  CHECK_FALSE(anon.empty());
  CHECK(h.client->anonymousId() == anon);
  CHECK(h.client->reset() == GTM_OK);
  CHECK(h.client->track("x") == GTM_OK);
  h.client.reset();
  CHECK_FALSE(fs::exists(sub));
  CHECK(h.snapshot().empty());
}

TEST_CASE("suppress_test_builds with production or unknown channel still sends") {
  for (auto ch : {gtmeasy::BuildChannel::Production, gtmeasy::BuildChannel::Unknown}) {
    Harness h;
    h.start_client([&](gtmeasy::Config& c) {
      c.suppressTestBuilds = true;
      c.buildChannel = ch;
    });
    CHECK_FALSE(h.client->isSuppressed());
    REQUIRE(h.client->track("feature.used") == GTM_OK);
    REQUIRE(h.client->flush(5000) == GTM_OK);
    CHECK(count_event(h.snapshot(), "feature.used") == 1);
  }
}

TEST_CASE("suppressed first open, then production with same storage sends first_open once") {
  Harness h;
  h.start_client([](gtmeasy::Config& c) {
    suppress_debug(c);
    c.appVersion = "1.0.0";
  });
  REQUIRE(h.client->trackFirstOpen() == GTM_OK);
  h.client.reset();
  CHECK(h.snapshot().empty());
  CHECK_FALSE(fs::exists(h.dir / "last_version"));

  for (int launch = 0; launch < 2; ++launch) {
    h.start_client([](gtmeasy::Config& c) {
      c.appVersion = "1.0.0";
      c.suppressTestBuilds = true;
      c.buildChannel = gtmeasy::BuildChannel::Production;
    });
    REQUIRE(h.client->trackFirstOpen() == GTM_OK);
    REQUIRE(h.client->flush(5000) == GTM_OK);
    h.client.reset();
  }
  const auto hits = h.snapshot();
  CHECK(count_event(hits, "app.first_open") == 1);
  CHECK(count_event(hits, "app.opened") == 2);
  CHECK(count_event(hits, "app.updated") == 0);
}

namespace {

struct LogCapture {
  std::vector<std::pair<gtm_log_level_t, std::string>> lines;
};

void capture_log(gtm_log_level_t level, const char* message, void* user) {
  static_cast<LogCapture*>(user)->lines.emplace_back(level, message ? message : "");
}

struct CApiFixture {
  fs::path dir;
  std::string dir_s;
  gtm_growth_config_t cfg;
  LogCapture logs;

  CApiFixture() {
    dir = fs::temp_directory_path() / ("gtm-capi-ext-" + gtmeasy::internal::uuid_v4());
    dir_s = dir.string();
    gtm_growth_config_init(&cfg);
    cfg.api_key = "secret-write-key";
    cfg.app = "a";
    cfg.cache_dir = dir_s.c_str();
    cfg.flush_interval_ms = 60000;
    cfg.base_url = "http://127.0.0.1:1";
    cfg.log_level = GTM_LOG_DEBUG;
    cfg.log_fn = capture_log;
    cfg.log_user_data = &logs;
  }
  ~CApiFixture() {
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  // Returns is_suppressed, or -1 when create failed.
  int create_and_probe(const gtm_growth_config_ext_t* ext, gtm_error_t* err = nullptr) {
    gtm_growth_t* client = nullptr;
    const gtm_error_t e = gtm_growth_create_ext(&cfg, ext, &client);
    if (err) {
      *err = e;
    }
    if (e != GTM_OK) {
      return -1;
    }
    const int s = gtm_growth_is_suppressed(client);
    gtm_growth_destroy(client);
    return s;
  }
};

} // namespace

TEST_CASE("C ext API: init, NULL ext, struct_size handling, invalid channel") {
  CApiFixture f;

  gtm_growth_config_ext_t ext;
  gtm_growth_config_ext_init(&ext);
  CHECK(ext.struct_size == sizeof(gtm_growth_config_ext_t));
  CHECK(ext.suppress_test_builds == 0);
  CHECK(ext.disabled == 0);
  CHECK(ext.build_channel == GTM_BUILD_CHANNEL_UNKNOWN);

  CHECK(f.create_and_probe(nullptr) == 0);
  CHECK(f.create_and_probe(&ext) == 0);

  gtm_error_t err = GTM_OK;
  gtm_growth_config_ext_t zero{};
  zero.disabled = 1;
  CHECK(f.create_and_probe(&zero, &err) == -1);
  CHECK(err == GTM_ERROR_INVALID_ARGUMENT);

  ext.suppress_test_builds = 1;
  ext.build_channel = GTM_BUILD_CHANNEL_TESTFLIGHT;
  CHECK(f.create_and_probe(&ext) == 1);

  // Older caller whose struct ends before `disabled`: later fields are ignored.
  gtm_growth_config_ext_t old_ext;
  gtm_growth_config_ext_init(&old_ext);
  old_ext.struct_size = static_cast<uint32_t>(offsetof(gtm_growth_config_ext_t, disabled));
  old_ext.suppress_test_builds = 1;
  old_ext.disabled = 1;
  old_ext.build_channel = GTM_BUILD_CHANNEL_DEBUG;
  CHECK(f.create_and_probe(&old_ext) == 0);

  // Ends before build_channel: disabled is honoured.
  old_ext.struct_size = static_cast<uint32_t>(offsetof(gtm_growth_config_ext_t, build_channel));
  CHECK(f.create_and_probe(&old_ext) == 1);
  old_ext.disabled = 0;
  CHECK(f.create_and_probe(&old_ext) == 0); // channel unread -> unknown -> not suppressed

  // Newer caller with a larger struct: known fields are read, the rest ignored.
  struct Bigger {
    gtm_growth_config_ext_t base;
    int future_field;
  } big{};
  gtm_growth_config_ext_init(&big.base);
  big.base.struct_size = static_cast<uint32_t>(sizeof(Bigger));
  big.base.suppress_test_builds = 1;
  big.base.build_channel = GTM_BUILD_CHANNEL_INTERNAL_TEST;
  big.future_field = 42;
  CHECK(f.create_and_probe(&big.base) == 1);

  // Out-of-range channel resolves to unknown, which is never suppressed.
  ext.build_channel = 99;
  CHECK(f.create_and_probe(&ext) == 0);
}

TEST_CASE("C ext API: one suppression log line without the write key") {
  CApiFixture f;
  gtm_growth_config_ext_t ext;
  gtm_growth_config_ext_init(&ext);
  ext.suppress_test_builds = 1;
  ext.build_channel = GTM_BUILD_CHANNEL_DEBUG;
  gtm_growth_t* client = nullptr;
  REQUIRE(gtm_growth_create_ext(&f.cfg, &ext, &client) == GTM_OK);
  CHECK(gtm_growth_build_channel(client) == GTM_BUILD_CHANNEL_DEBUG);
  CHECK(gtm_growth_track(client, "e", nullptr) == GTM_OK);
  CHECK(gtm_growth_flush(client, 500) == GTM_OK);
  gtm_growth_destroy(client);

  int suppress_lines = 0;
  for (const auto& l : f.logs.lines) {
    CHECK(l.second.find("secret-write-key") == std::string::npos);
    if (l.second.find("suppressing events (channel=debug)") != std::string::npos) {
      ++suppress_lines;
      CHECK(l.first == GTM_LOG_INFO);
    }
  }
  CHECK(suppress_lines == 1);
  CHECK_FALSE(fs::exists(f.dir));
  CHECK(gtm_growth_is_suppressed(nullptr) == 0);
}
