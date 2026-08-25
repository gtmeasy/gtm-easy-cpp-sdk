#include "util.hpp"

#include <doctest/doctest.h>
#include <regex>
#include <set>
#include <string>

using namespace gtmeasy::internal;

TEST_CASE("UUID v4 format and uniqueness") {
  const std::regex re(
      "^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$");
  std::set<std::string> seen;
  for (int i = 0; i < 50; ++i) {
    const std::string id = uuid_v4();
    CHECK(std::regex_match(id, re));
    CHECK(seen.insert(id).second);
  }
}

TEST_CASE("RFC 3339 formatting") {
  const std::string ts = rfc3339_now();
  const std::regex re("^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}\\.\\d{3}Z$");
  CHECK(std::regex_match(ts, re));
}
