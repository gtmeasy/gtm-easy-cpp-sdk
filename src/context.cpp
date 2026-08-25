#include "context.hpp"

#include "gtmeasy/version.h"
#include "util.hpp"

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace gtmeasy {
namespace internal {
namespace {

#if defined(__APPLE__)
std::string cf_string(CFStringRef ref) {
  if (!ref) {
    return {};
  }
  const char* fast = CFStringGetCStringPtr(ref, kCFStringEncodingUTF8);
  if (fast) {
    return std::string(fast);
  }
  const CFIndex len = CFStringGetLength(ref);
  const CFIndex max = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
  std::string out(static_cast<size_t>(max), '\0');
  if (CFStringGetCString(ref, out.data(), max, kCFStringEncodingUTF8)) {
    out.resize(std::strlen(out.c_str()));
    return out;
  }
  return {};
}
#endif

#if defined(_WIN32)
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

std::string windows_iana_zone(const std::string& win_name) {
  static const struct {
    const char* win;
    const char* iana;
  } kMap[] = {
      {"Pacific Standard Time", "America/Los_Angeles"},
      {"Mountain Standard Time", "America/Denver"},
      {"US Mountain Standard Time", "America/Phoenix"},
      {"Central Standard Time", "America/Chicago"},
      {"Eastern Standard Time", "America/New_York"},
      {"Alaskan Standard Time", "America/Anchorage"},
      {"Hawaiian Standard Time", "Pacific/Honolulu"},
      {"GMT Standard Time", "Europe/London"},
      {"W. Europe Standard Time", "Europe/Berlin"},
      {"Romance Standard Time", "Europe/Paris"},
      {"Central Europe Standard Time", "Europe/Budapest"},
      {"Central European Standard Time", "Europe/Warsaw"},
      {"GTB Standard Time", "Europe/Bucharest"},
      {"E. Europe Standard Time", "Europe/Chisinau"},
      {"FLE Standard Time", "Europe/Kiev"},
      {"Russian Standard Time", "Europe/Moscow"},
      {"China Standard Time", "Asia/Shanghai"},
      {"Tokyo Standard Time", "Asia/Tokyo"},
      {"Korea Standard Time", "Asia/Seoul"},
      {"Singapore Standard Time", "Asia/Singapore"},
      {"India Standard Time", "Asia/Kolkata"},
      {"AUS Eastern Standard Time", "Australia/Sydney"},
      {"E. Australia Standard Time", "Australia/Brisbane"},
      {"W. Australia Standard Time", "Australia/Perth"},
      {"AUS Central Standard Time", "Australia/Adelaide"},
      {"New Zealand Standard Time", "Pacific/Auckland"},
      {"UTC", "UTC"},
      {"SA Pacific Standard Time", "America/Bogota"},
      {"Central America Standard Time", "America/Guatemala"},
      {"US Eastern Standard Time", "America/Indiana/Indianapolis"},
      {"Atlantic Standard Time", "America/Halifax"},
      {"Pacific SA Standard Time", "America/Santiago"},
      {"E. South America Standard Time", "America/Sao_Paulo"},
      {"Arabian Standard Time", "Asia/Dubai"},
      {"Middle East Standard Time", "Asia/Beirut"},
      {"Israel Standard Time", "Asia/Jerusalem"},
      {"Iran Standard Time", "Asia/Tehran"},
      {"SE Asia Standard Time", "Asia/Bangkok"},
      {"Taipei Standard Time", "Asia/Taipei"},
      {"Canada Central Standard Time", "America/Regina"},
  };
  for (const auto& row : kMap) {
    if (win_name == row.win) {
      return row.iana;
    }
  }
  return win_name;
}

void fill_windows(DeviceContext* ctx) {
  struct RtlOsVersionInfo {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
  };
  using RtlGetVersionFn = LONG(WINAPI*)(RtlOsVersionInfo*);
  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll) {
    auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (fn) {
      RtlOsVersionInfo info{};
      info.dwOSVersionInfoSize = sizeof(info);
      if (fn(&info) == 0) {
        ctx->os_version = std::to_string(info.dwMajorVersion) + "." +
                          std::to_string(info.dwMinorVersion) + "." +
                          std::to_string(info.dwBuildNumber);
      }
    }
  }

  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &key) ==
      ERROR_SUCCESS) {
    auto read_reg = [&](const wchar_t* value) {
      wchar_t buf[256];
      DWORD size = sizeof(buf);
      DWORD type = 0;
      if (RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size) ==
              ERROR_SUCCESS &&
          (type == REG_SZ || type == REG_EXPAND_SZ)) {
        return wide_to_utf8(buf);
      }
      return std::string();
    };
    ctx->device_manufacturer = read_reg(L"SystemManufacturer");
    ctx->device_model = read_reg(L"SystemProductName");
    RegCloseKey(key);
  }

  MEMORYSTATUSEX mem{};
  mem.dwLength = sizeof(mem);
  if (GlobalMemoryStatusEx(&mem)) {
    ctx->physical_memory_bytes = static_cast<uint64_t>(mem.ullTotalPhys);
  }

  wchar_t locale[85];
  if (GetUserDefaultLocaleName(locale, 85) > 0) {
    ctx->locale = wide_to_utf8(locale);
  }

  DYNAMIC_TIME_ZONE_INFORMATION tz{};
  GetDynamicTimeZoneInformation(&tz);
  ctx->timezone = windows_iana_zone(wide_to_utf8(tz.TimeZoneKeyName));
}
#endif

#if defined(__APPLE__)
std::string sysctl_string(const char* name) {
  char buf[256];
  size_t len = sizeof(buf);
  if (sysctlbyname(name, buf, &len, nullptr, 0) == 0 && len > 0) {
    if (buf[len - 1] == '\0') {
      return std::string(buf);
    }
    return std::string(buf, len);
  }
  return {};
}

void fill_macos(DeviceContext* ctx) {
  ctx->os_version = sysctl_string("kern.osproductversion");
  ctx->device_model = sysctl_string("hw.model");
  ctx->device_manufacturer = "Apple";
  uint64_t mem = 0;
  size_t len = sizeof(mem);
  if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0) {
    ctx->physical_memory_bytes = mem;
  }

  CFLocaleRef locale = CFLocaleCopyCurrent();
  if (locale) {
    CFStringRef ident = static_cast<CFStringRef>(CFLocaleGetValue(locale, kCFLocaleIdentifier));
    ctx->locale = cf_string(ident);
    CFRelease(locale);
  }
  if (ctx->locale.empty()) {
    const char* lang = std::getenv("LANG");
    if (lang) {
      ctx->locale = lang;
      const auto dot = ctx->locale.find('.');
      if (dot != std::string::npos) {
        ctx->locale.resize(dot);
      }
      for (char& c : ctx->locale) {
        if (c == '_') {
          c = '-';
        }
      }
    }
  }

  CFTimeZoneRef tz = CFTimeZoneCopySystem();
  if (tz) {
    CFStringRef name = CFTimeZoneGetName(tz);
    ctx->timezone = cf_string(name);
    CFRelease(tz);
  }
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
std::string read_trimmed(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    return {};
  }
  std::string line;
  std::getline(in, line);
  return trim(line);
}

void fill_linux(DeviceContext* ctx) {
  std::ifstream os("/etc/os-release");
  std::string line;
  while (std::getline(os, line)) {
    if (line.rfind("PRETTY_NAME=", 0) == 0) {
      ctx->os_version = line.substr(12);
      if (!ctx->os_version.empty() && ctx->os_version.front() == '"') {
        ctx->os_version.erase(ctx->os_version.begin());
      }
      if (!ctx->os_version.empty() && ctx->os_version.back() == '"') {
        ctx->os_version.pop_back();
      }
      break;
    }
  }
  if (ctx->os_version.empty()) {
    struct utsname u {};
    if (uname(&u) == 0) {
      ctx->os_version = std::string(u.sysname) + " " + u.release;
    }
  }
  ctx->device_manufacturer = read_trimmed("/sys/devices/virtual/dmi/id/sys_vendor");
  ctx->device_model = read_trimmed("/sys/devices/virtual/dmi/id/product_name");
  struct sysinfo info {};
  if (sysinfo(&info) == 0) {
    ctx->physical_memory_bytes = static_cast<uint64_t>(info.totalram) * info.mem_unit;
  }
  const char* lang = std::getenv("LANG");
  if (lang) {
    ctx->locale = lang;
    const auto dot = ctx->locale.find('.');
    if (dot != std::string::npos) {
      ctx->locale.resize(dot);
    }
    for (char& c : ctx->locale) {
      if (c == '_') {
        c = '-';
      }
    }
  }
  ctx->timezone = read_trimmed("/etc/timezone");
  if (ctx->timezone.empty()) {
    char buf[512];
    const ssize_t n = readlink("/etc/localtime", buf, sizeof(buf) - 1);
    if (n > 0) {
      buf[n] = '\0';
      const std::string path(buf);
      const std::string marker = "zoneinfo/";
      const auto pos = path.find(marker);
      if (pos != std::string::npos) {
        ctx->timezone = path.substr(pos + marker.size());
      }
    }
  }
}
#endif

} // namespace

const char* platform_name() {
#if defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#else
  return "linux";
#endif
}

DeviceContext snapshot_context() {
  DeviceContext ctx;
  ctx.sdk = "cpp";
  ctx.sdk_version = GTMEASY_GROWTH_VERSION;
  ctx.platform = platform_name();
  ctx.hw_concurrency = std::thread::hardware_concurrency();
#if defined(_WIN32)
  fill_windows(&ctx);
#elif defined(__APPLE__)
  fill_macos(&ctx);
#else
  fill_linux(&ctx);
#endif
  return ctx;
}

nlohmann::json context_to_json(const DeviceContext& ctx) {
  nlohmann::json j = nlohmann::json::object();
  j["sdk"] = ctx.sdk;
  j["sdk_version"] = ctx.sdk_version;
  j["platform"] = ctx.platform;
  if (!ctx.os_version.empty()) {
    j["os_version"] = ctx.os_version;
  }
  if (!ctx.device_manufacturer.empty()) {
    j["device_manufacturer"] = ctx.device_manufacturer;
  }
  if (!ctx.device_model.empty()) {
    j["device_model"] = ctx.device_model;
  }
  if (ctx.physical_memory_bytes > 0) {
    j["physical_memory_bytes"] = ctx.physical_memory_bytes;
  }
  if (ctx.hw_concurrency > 0) {
    j["hw_concurrency"] = ctx.hw_concurrency;
  }
  return j;
}

} // namespace internal
} // namespace gtmeasy
