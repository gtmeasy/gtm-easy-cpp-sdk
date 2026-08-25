#ifndef GTMEASY_GROWTH_EVENTS_H
#define GTMEASY_GROWTH_EVENTS_H

namespace gtmeasy {
namespace events {

constexpr const char* app_first_open = "app.first_open";
constexpr const char* app_opened = "app.opened";
constexpr const char* app_updated = "app.updated";
constexpr const char* user_created = "user.created";
constexpr const char* session_started = "session.started";
constexpr const char* session_ended = "session.ended";
constexpr const char* acquisition_attribution_resolved = "acquisition.attribution_resolved";
constexpr const char* acquisition_app_download = "acquisition.app_download";
constexpr const char* acquisition_user_registered = "acquisition.user_registered";
constexpr const char* app_store_product_page_viewed = "app_store.product_page_viewed";
constexpr const char* app_store_install_attributed = "app_store.install_attributed";
constexpr const char* onboarding_started = "onboarding.started";
constexpr const char* onboarding_completed = "onboarding.completed";
constexpr const char* onboarding_step_viewed = "onboarding.step_viewed";
constexpr const char* onboarding_step_completed = "onboarding.step_completed";
constexpr const char* onboarding_step_skipped = "onboarding.step_skipped";
constexpr const char* onboarding_goal_selected = "onboarding.goal_selected";
constexpr const char* onboarding_permission_prompted = "onboarding.permission_prompted";
constexpr const char* onboarding_permission_granted = "onboarding.permission_granted";
constexpr const char* onboarding_permission_denied = "onboarding.permission_denied";
constexpr const char* survey_shown = "survey.shown";
constexpr const char* survey_started = "survey.started";
constexpr const char* survey_completed = "survey.completed";
constexpr const char* survey_dismissed = "survey.dismissed";
constexpr const char* activation_completed = "activation.completed";
constexpr const char* feature_used = "feature.used";
constexpr const char* feature_first_used = "feature.first_used";
constexpr const char* page_viewed = "page.viewed";
constexpr const char* button_clicked = "button.clicked";
constexpr const char* goal_completed = "goal.completed";
constexpr const char* paywall_opened = "paywall.opened";
constexpr const char* paywall_closed = "paywall.closed";
constexpr const char* paywall_upgrade_clicked = "paywall.upgrade_clicked";
constexpr const char* paywall_upgrade_cancelled = "paywall.upgrade_cancelled";
constexpr const char* paywall_products_requested = "paywall.products_requested";
constexpr const char* paywall_products_loaded = "paywall.products_loaded";
constexpr const char* paywall_products_failed = "paywall.products_failed";
constexpr const char* paywall_offer_eligible = "paywall.offer_eligible";
constexpr const char* paywall_plan_selected = "paywall.plan_selected";
constexpr const char* paywall_restore_clicked = "paywall.restore_clicked";
constexpr const char* paywall_restore_completed = "paywall.restore_completed";
constexpr const char* paywall_restore_failed = "paywall.restore_failed";
constexpr const char* purchase_started = "purchase.started";
constexpr const char* purchase_completed = "purchase.completed";
constexpr const char* purchase_failed = "purchase.failed";
constexpr const char* purchase_refunded = "purchase.refunded";
constexpr const char* trial_eligible = "trial.eligible";
constexpr const char* trial_ineligible = "trial.ineligible";
constexpr const char* trial_started = "trial.started";
constexpr const char* trial_converted = "trial.converted";
constexpr const char* trial_cancelled = "trial.cancelled";
constexpr const char* trial_expired = "trial.expired";
constexpr const char* trial_billing_retry_started = "trial.billing_retry_started";
constexpr const char* subscription_started = "subscription.started";
constexpr const char* subscription_renewed = "subscription.renewed";
constexpr const char* subscription_cancelled = "subscription.cancelled";
constexpr const char* subscription_expired = "subscription.expired";
constexpr const char* subscription_billing_issue = "subscription.billing_issue";
constexpr const char* subscription_grace_period_started = "subscription.grace_period_started";
constexpr const char* subscription_recovered = "subscription.recovered";
constexpr const char* subscription_refunded = "subscription.refunded";
constexpr const char* subscription_price_increase_accepted = "subscription.price_increase_accepted";
constexpr const char* subscription_price_increase_declined = "subscription.price_increase_declined";
constexpr const char* revenue_received = "revenue.received";
constexpr const char* referral_sent = "referral.sent";
constexpr const char* referral_accepted = "referral.accepted";
constexpr const char* review_prompted = "review.prompted";
constexpr const char* review_submitted = "review.submitted";
constexpr const char* notification_received = "notification.received";
constexpr const char* notification_opened = "notification.opened";
constexpr const char* error_occurred = "error.occurred";

} // namespace events
} // namespace gtmeasy

#endif
