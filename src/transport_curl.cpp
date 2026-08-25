#include "transport.hpp"

#include "util.hpp"

#include <curl/curl.h>
#include <memory>
#include <mutex>
#include <vector>

namespace gtmeasy {
namespace internal {
namespace {

struct CurlOnce {
  CurlOnce() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlOnce() { curl_global_cleanup(); }
};

void ensure_curl() {
  static CurlOnce once;
  (void)once;
}

struct WriteCtx {
  std::string data;
};

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* ctx = static_cast<WriteCtx*>(userdata);
  ctx->data.append(ptr, size * nmemb);
  return size * nmemb;
}

struct HeaderCtx {
  int retry_after = 0;
};

size_t header_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
  auto* ctx = static_cast<HeaderCtx*>(userdata);
  const std::string line(buffer, size * nitems);
  const auto colon = line.find(':');
  if (colon != std::string::npos) {
    std::string name = to_lower(trim(line.substr(0, colon)));
    std::string value = trim(line.substr(colon + 1));
    if (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
      value = trim(value);
    }
    if (name == "retry-after") {
      ctx->retry_after = parse_retry_after(value, 30);
    }
  }
  return size * nitems;
}

class CurlTransport final : public Transport {
 public:
  HttpResponse post(const std::string& url,
                    const std::map<std::string, std::string>& headers,
                    const std::string& body,
                    int timeout_ms) override {
    ensure_curl();
    HttpResponse out;
    CURL* curl = curl_easy_init();
    if (!curl) {
      out.error = "curl_easy_init failed";
      return out;
    }

    WriteCtx write;
    HeaderCtx hdr;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hdr);
    const long timeout = timeout_ms > 0 ? timeout_ms : 10000;
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "gtmeasy-growth-cpp/0.1.0");

    struct curl_slist* list = nullptr;
    std::vector<std::string> owned;
    owned.reserve(headers.size());
    for (const auto& kv : headers) {
      owned.push_back(kv.first + ": " + kv.second);
      list = curl_slist_append(list, owned.back().c_str());
    }
    if (list) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    }

    const CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
      out.error = curl_easy_strerror(rc);
      if (rc == CURLE_OPERATION_TIMEDOUT) {
        out.status = 0;
      }
    } else {
      long status = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
      out.status = static_cast<int>(status);
      out.body = std::move(write.data);
      out.retry_after_seconds = hdr.retry_after;
    }

    curl_slist_free_all(list);
    curl_easy_cleanup(curl);
    return out;
  }
};

} // namespace

std::unique_ptr<Transport> create_transport() {
  return std::make_unique<CurlTransport>();
}

} // namespace internal
} // namespace gtmeasy
