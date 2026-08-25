#include <gtmeasy/events.h>
#include <gtmeasy/growth.h>

#include <cstdlib>
#include <iostream>

int main() {
  const char* key = std::getenv("GTM_GROWTH_KEY");
  const char* app = std::getenv("GTM_GROWTH_APP");
  const char* base = std::getenv("GTM_GROWTH_BASE_URL");
  if (!key || !app) {
    std::cerr << "Set GTM_GROWTH_KEY and GTM_GROWTH_APP\n";
    return 1;
  }

  gtmeasy::Config cfg;
  cfg.apiKey = key;
  cfg.app = app;
  if (base) {
    cfg.baseUrl = base;
  }
  cfg.cacheDir = gtmeasy::defaultCacheDir("GtmEasy", "example");

  auto analytics = gtmeasy::GrowthAnalytics::create(cfg);
  if (!analytics) {
    std::cerr << "create failed: " << gtm_error_name(gtmeasy::last_error()) << "\n";
    return 1;
  }

  analytics->trackFirstOpen();
  analytics->identify("demo-user", {{"plan", "free"}});
  analytics->track(gtmeasy::events::feature_used, {{"feature", "console"}});
  analytics->flush(5000);
  std::cout << "anonymous_id=" << analytics->anonymousId() << "\n";
  return 0;
}
