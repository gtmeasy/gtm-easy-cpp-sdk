#include <gtmeasy/growth_c.h>
#include <gtmeasy/version.h>

#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

TEST_CASE("C API NULL/invalid arguments return GTM_ERROR_INVALID_ARGUMENT") {
  CHECK(gtm_growth_create(nullptr, nullptr) == GTM_ERROR_INVALID_ARGUMENT);

  gtm_growth_config_t cfg;
  gtm_growth_config_init(&cfg);
  gtm_growth_t* client = nullptr;
  CHECK(gtm_growth_create(&cfg, &client) == GTM_ERROR_INVALID_ARGUMENT);

  cfg.api_key = "k";
  cfg.app = "a";
  CHECK(gtm_growth_create(&cfg, &client) == GTM_ERROR_INVALID_ARGUMENT);

  CHECK(gtm_growth_identify(nullptr, "u", nullptr, nullptr, nullptr) == GTM_ERROR_NOT_INITIALIZED);
  CHECK(gtm_growth_track(nullptr, "e", nullptr) == GTM_ERROR_NOT_INITIALIZED);

  const auto dir = fs::temp_directory_path() / "gtm-capi";
  fs::create_directories(dir);
  const std::string dir_s = dir.string();
  cfg.cache_dir = dir_s.c_str();
  cfg.flush_interval_ms = 60000;
  cfg.base_url = "http://127.0.0.1:1";
  REQUIRE(gtm_growth_create(&cfg, &client) == GTM_OK);
  CHECK(gtm_growth_identify(client, nullptr, nullptr, nullptr, nullptr) == GTM_ERROR_INVALID_ARGUMENT);
  CHECK(gtm_growth_identify(client, "", nullptr, nullptr, nullptr) == GTM_ERROR_INVALID_ARGUMENT);
  CHECK(gtm_growth_track(client, nullptr, nullptr) == GTM_ERROR_INVALID_ARGUMENT);
  CHECK(gtm_growth_track(client, "", nullptr) == GTM_ERROR_INVALID_ARGUMENT);
  CHECK(gtm_growth_record_click_id(client, nullptr, "x") == GTM_ERROR_INVALID_ARGUMENT);
  gtm_growth_destroy(client);
  fs::remove_all(dir);
}

TEST_CASE("C API invalid JSON returns GTM_ERROR_INVALID_PAYLOAD") {
  const auto dir = fs::temp_directory_path() / "gtm-capi-json";
  fs::create_directories(dir);
  const std::string dir_s = dir.string();
  gtm_growth_config_t cfg;
  gtm_growth_config_init(&cfg);
  cfg.api_key = "k";
  cfg.app = "a";
  cfg.cache_dir = dir_s.c_str();
  cfg.flush_interval_ms = 60000;
  cfg.base_url = "http://127.0.0.1:1";
  gtm_growth_t* client = nullptr;
  REQUIRE(gtm_growth_create(&cfg, &client) == GTM_OK);
  CHECK(gtm_growth_track(client, "e", "not-json") == GTM_ERROR_INVALID_PAYLOAD);
  CHECK(gtm_growth_track(client, "e", "[1,2]") == GTM_ERROR_INVALID_PAYLOAD);
  CHECK(gtm_growth_identify(client, "u", nullptr, nullptr, "[]") == GTM_ERROR_INVALID_PAYLOAD);
  CHECK(gtm_growth_track(client, "e", "{}") == GTM_OK);
  gtm_growth_destroy(client);
  fs::remove_all(dir);
}

TEST_CASE("gtm_error_name round-trips every enum value") {
  const std::vector<gtm_error_t> vals = {GTM_OK,
                                         GTM_ERROR_UNKNOWN,
                                         GTM_ERROR_INVALID_ARGUMENT,
                                         GTM_ERROR_NOT_INITIALIZED,
                                         GTM_ERROR_NETWORK,
                                         GTM_ERROR_INVALID_CREDENTIALS,
                                         GTM_ERROR_RATE_LIMITED,
                                         GTM_ERROR_SERVER,
                                         GTM_ERROR_INVALID_PAYLOAD,
                                         GTM_ERROR_STORAGE,
                                         GTM_ERROR_TIMEOUT};
  for (auto v : vals) {
    const char* name = gtm_error_name(v);
    REQUIRE(name != nullptr);
    CHECK(name[0] != '\0');
  }
  CHECK(std::string(gtm_error_name(GTM_OK)) == "ok");
  CHECK(std::string(gtm_error_name(GTM_ERROR_NETWORK)) == "network_error");
  CHECK(std::string(gtm_error_name(GTM_ERROR_INVALID_CREDENTIALS)) == "invalid_credentials");
  CHECK(std::string(gtm_growth_version()) == GTMEASY_GROWTH_VERSION);
}
