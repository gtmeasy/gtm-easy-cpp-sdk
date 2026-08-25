#include "transport.hpp"

#include "util.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <vector>

namespace gtmeasy {
namespace internal {
namespace {

std::wstring utf8_to_wide(const std::string& s) {
  if (s.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  if (n <= 1) {
    return {};
  }
  std::wstring out(static_cast<size_t>(n - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
  return out;
}

std::string wide_to_utf8(const std::wstring& w) {
  if (w.empty()) {
    return {};
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) {
    return {};
  }
  std::string out(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
  return out;
}

class WinHttpHandle {
 public:
  explicit WinHttpHandle(HINTERNET h = nullptr) : h_(h) {}
  ~WinHttpHandle() { reset(); }
  WinHttpHandle(const WinHttpHandle&) = delete;
  WinHttpHandle& operator=(const WinHttpHandle&) = delete;
  WinHttpHandle(WinHttpHandle&& other) noexcept : h_(other.h_) { other.h_ = nullptr; }
  WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
    if (this != &other) {
      reset();
      h_ = other.h_;
      other.h_ = nullptr;
    }
    return *this;
  }
  HINTERNET get() const { return h_; }
  void reset() {
    if (h_) {
      WinHttpCloseHandle(h_);
      h_ = nullptr;
    }
  }

 private:
  HINTERNET h_;
};

class WinHttpTransport final : public Transport {
 public:
  HttpResponse post(const std::string& url,
                    const std::map<std::string, std::string>& headers,
                    const std::string& body,
                    int timeout_ms) override {
    HttpResponse out;
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t scheme[16];
    wchar_t host[256];
    wchar_t path[2048];
    wchar_t extra[2048];
    parts.lpszScheme = scheme;
    parts.dwSchemeLength = 16;
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 2048;

    const std::wstring wurl = utf8_to_wide(url);
    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &parts)) {
      out.error = "WinHttpCrackUrl failed";
      return out;
    }

    WinHttpHandle session(WinHttpOpen(L"gtmeasy-growth-cpp/0.1.0",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS,
                                      0));
    if (!session.get()) {
      out.error = "WinHttpOpen failed";
      return out;
    }

    const int timeout = timeout_ms > 0 ? timeout_ms : 10000;
    WinHttpSetTimeouts(session.get(), timeout, timeout, timeout, timeout);

    INTERNET_PORT port = parts.nPort;
    WinHttpHandle connect(WinHttpConnect(session.get(), host, port, 0));
    if (!connect.get()) {
      out.error = "WinHttpConnect failed";
      return out;
    }

    std::wstring object(path, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength > 0) {
      object.append(extra, parts.dwExtraInfoLength);
    }

    DWORD flags = 0;
    if (parts.nScheme == INTERNET_SCHEME_HTTPS) {
      flags |= WINHTTP_FLAG_SECURE;
    }

    WinHttpHandle request(WinHttpOpenRequest(connect.get(),
                                             L"POST",
                                             object.c_str(),
                                             nullptr,
                                             WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             flags));
    if (!request.get()) {
      out.error = "WinHttpOpenRequest failed";
      return out;
    }

    std::wstring header_block;
    for (const auto& kv : headers) {
      header_block += utf8_to_wide(kv.first);
      header_block += L": ";
      header_block += utf8_to_wide(kv.second);
      header_block += L"\r\n";
    }
    if (!header_block.empty()) {
      if (!WinHttpAddRequestHeaders(request.get(),
                                    header_block.c_str(),
                                    static_cast<DWORD>(header_block.size()),
                                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
        out.error = "WinHttpAddRequestHeaders failed";
        return out;
      }
    }

    const BOOL sent = WinHttpSendRequest(request.get(),
                                         WINHTTP_NO_ADDITIONAL_HEADERS,
                                         0,
                                         body.empty() ? WINHTTP_NO_REQUEST_DATA
                                                      : const_cast<LPVOID>(static_cast<LPCVOID>(body.data())),
                                         static_cast<DWORD>(body.size()),
                                         static_cast<DWORD>(body.size()),
                                         0);
    if (!sent) {
      out.error = "WinHttpSendRequest failed";
      return out;
    }
    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
      out.error = "WinHttpReceiveResponse failed";
      return out;
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(request.get(),
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status,
                        &status_size,
                        WINHTTP_NO_HEADER_INDEX);
    out.status = static_cast<int>(status);

    wchar_t retry_after[64] = {};
    DWORD retry_size = sizeof(retry_after);
    if (WinHttpQueryHeaders(request.get(),
                            WINHTTP_QUERY_CUSTOM,
                            L"Retry-After",
                            retry_after,
                            &retry_size,
                            WINHTTP_NO_HEADER_INDEX)) {
      out.retry_after_seconds = parse_retry_after(wide_to_utf8(std::wstring(retry_after)), 30);
    }

    for (;;) {
      DWORD avail = 0;
      if (!WinHttpQueryDataAvailable(request.get(), &avail)) {
        out.error = "WinHttpQueryDataAvailable failed";
        break;
      }
      if (avail == 0) {
        break;
      }
      std::vector<char> buf(avail);
      DWORD read = 0;
      if (!WinHttpReadData(request.get(), buf.data(), avail, &read)) {
        out.error = "WinHttpReadData failed";
        break;
      }
      if (read == 0) {
        break;
      }
      out.body.append(buf.data(), read);
    }
    return out;
  }
};

} // namespace

std::unique_ptr<Transport> create_transport() {
  return std::unique_ptr<Transport>(new WinHttpTransport());
}

} // namespace internal
} // namespace gtmeasy
