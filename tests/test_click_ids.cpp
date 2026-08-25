#include "click_ids.hpp"
#include "storage.hpp"
#include "util.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

TEST_CASE("Click ids URL parsing, TTL expiry, fbc/fbp derivation") {
  const auto dir = fs::temp_directory_path() / ("gtm-clicks-" + gtmeasy::internal::uuid_v4());
  fs::create_directories(dir);
  gtmeasy::internal::Storage storage(dir.string());
  REQUIRE(storage.ensure_dir(nullptr));
  gtmeasy::internal::ClickIdStore store(&storage);

  const int n = store.capture_url("https://example.com/app?gclid=G123&fbclid=FB99&utm_source=x");
  CHECK(n == 2);

  nlohmann::json ctx = nlohmann::json::object();
  store.merge_into(ctx);
  CHECK(ctx["gclid"] == "G123");
  CHECK(ctx["fbclid"] == "FB99");
  REQUIRE(ctx.contains("fbc"));
  const std::string fbc = ctx["fbc"].get<std::string>();
  CHECK(fbc.find("fb.1.") == 0);
  CHECK(fbc.find("FB99") != std::string::npos);
  REQUIRE(ctx.contains("fbp"));
  const std::string fbp = ctx["fbp"].get<std::string>();
  CHECK(fbp.find("fb.1.") == 0);

  nlohmann::json on_disk;
  REQUIRE(storage.read_json("click_ids.json", &on_disk, nullptr));
  on_disk["gclid"]["ts"] = 1;
  REQUIRE(storage.write_json("click_ids.json", on_disk, nullptr));

  gtmeasy::internal::ClickIdStore reloaded(&storage);
  reloaded.load();
  nlohmann::json ctx2 = nlohmann::json::object();
  reloaded.merge_into(ctx2);
  CHECK_FALSE(ctx2.contains("gclid"));
  CHECK(ctx2.contains("fbclid"));

  fs::remove_all(dir);
}
