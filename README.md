# GTM Easy C++ SDK

First-party C++ SDK for [GTM Easy](https://gtmeasy.com) growth analytics, attribution, surveys, and paywall-funnel events. Sends events to the GTM Easy ingestion API, identifies users, persists an anonymous ID across launches, and captures system context (OS version, device model, locale, timezone).

**Windows desktop first** (Win32 / Qt / Electron native modules / game engines), designed to be portable to macOS and Linux hosts.

Sibling SDKs: [Apple (Swift)](https://github.com/gtmeasy/gtm-easy-apple-sdk) · [Android (Kotlin)](https://github.com/gtmeasy/gtm-easy-android-sdk) · [Web (TypeScript)](https://github.com/gtmeasy/gtm-easy-js-sdk) · [Flutter (Dart)](https://github.com/gtmeasy/gtm-easy-flutter-sdk)

## Status

`0.1.0` — scaffold. API surface, build system, and the generated OpenAPI client are being designed; nothing is published yet.

## Planned layout

```
include/gtmeasy/   public headers (C++17, stable C API shell planned)
src/               SDK implementation (event queue, persistence, HTTP transport)
generated/         OpenAPI-generated API client (do not edit by hand)
examples/          minimal Win32 / console samples
tests/
```

## Planned usage

```cpp
#include <gtmeasy/growth.h>

auto analytics = gtmeasy::GrowthAnalytics::create({ .apiKey = "gtm_growth_..." });
analytics->identify("user_123", {{"plan", "pro"}});
analytics->track("paywall.shown", {{"placement", "onboarding"}});
analytics->flush();
```

## License

MIT © GTM Easy — <hi@gtmeasy.com>
