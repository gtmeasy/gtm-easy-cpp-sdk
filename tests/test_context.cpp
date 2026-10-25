#include "context.hpp"

#include <gtmeasy/version.h>

#include <string>

#include <doctest/doctest.h>

TEST_CASE("Context snapshot has sdk, sdk_version, platform and non-empty os_version") {
  const auto snap = gtmeasy::internal::snapshot_context();
  CHECK(snap.sdk == "cpp");
  CHECK(snap.sdk_version == std::string(GTMEASY_GROWTH_VERSION));
  CHECK(snap.platform == gtmeasy::internal::platform_name());
  CHECK_FALSE(snap.os_version.empty());
  const auto j = gtmeasy::internal::context_to_json(snap);
  CHECK(j["sdk"] == "cpp");
  CHECK(j["sdk_version"] == std::string(GTMEASY_GROWTH_VERSION));
  CHECK(j["platform"] == snap.platform);
  CHECK(j["os_version"] == snap.os_version);
}
