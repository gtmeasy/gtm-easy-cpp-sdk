#pragma once

#include <map>
#include <memory>
#include <string>

namespace gtmeasy {
namespace internal {

struct HttpResponse {
  int status = 0;
  std::string body;
  int retry_after_seconds = 0;
  std::string error;
};

class Transport {
 public:
  virtual ~Transport() = default;
  virtual HttpResponse post(const std::string& url,
                            const std::map<std::string, std::string>& headers,
                            const std::string& body,
                            int timeout_ms) = 0;
};

std::unique_ptr<Transport> create_transport();

} // namespace internal
} // namespace gtmeasy
