#include "queue.hpp"

#include "util.hpp"

namespace gtmeasy {
namespace internal {

Queue::Queue(Storage* storage, int max_queue_size)
    : storage_(storage), max_queue_size_(max_queue_size > 0 ? max_queue_size : 1000) {
  std::vector<QueueItem> items;
  std::string err;
  storage_->read_queue(&items, &err);
  count_ = static_cast<int>(items.size());
}

bool Queue::enqueue(const std::string& kind, const nlohmann::json& body, std::string* error) {
  nlohmann::json line;
  line["kind"] = kind;
  line["body"] = body;
  const std::string dumped = line.dump();

  std::lock_guard<std::mutex> lock(mu_);
  if (!storage_->append_queue_line(dumped, error)) {
    return false;
  }
  ++count_;
  if (count_ > max_queue_size_) {
    std::vector<QueueItem> items;
    storage_->read_queue(&items, error);
    const int overflow = static_cast<int>(items.size()) - max_queue_size_;
    if (overflow > 0) {
      items.erase(items.begin(), items.begin() + overflow);
      dropped_ += overflow;
      if (!storage_->rewrite_queue(items, error)) {
        return false;
      }
      count_ = static_cast<int>(items.size());
    }
  }
  cv_.notify_all();
  return true;
}

int Queue::size() {
  std::lock_guard<std::mutex> lock(mu_);
  return count_;
}

std::vector<QueueItem> Queue::snapshot() {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<QueueItem> items;
  std::string err;
  storage_->read_queue(&items, &err);
  count_ = static_cast<int>(items.size());
  return items;
}

bool Queue::compact_keep(const std::vector<char>& keep_prefix, int prefix_len, std::string* error) {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<QueueItem> items;
  if (!storage_->read_queue(&items, error)) {
    return false;
  }
  std::vector<QueueItem> kept;
  kept.reserve(items.size());
  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    if (i < prefix_len) {
      const bool keep = i < static_cast<int>(keep_prefix.size()) && keep_prefix[static_cast<size_t>(i)];
      if (keep) {
        kept.push_back(items[static_cast<size_t>(i)]);
      }
    } else {
      kept.push_back(items[static_cast<size_t>(i)]);
    }
  }
  if (!storage_->rewrite_queue(kept, error)) {
    return false;
  }
  count_ = static_cast<int>(kept.size());
  cv_.notify_all();
  return true;
}

void Queue::notify() { cv_.notify_all(); }

bool Queue::wait_for_work(int interval_ms, int flush_at, const std::function<bool()>& extra_wake) {
  std::unique_lock<std::mutex> lock(mu_);
  const auto dur = std::chrono::milliseconds(interval_ms > 0 ? interval_ms : 30000);
  return cv_.wait_for(lock, dur, [&] {
    return extra_wake() || (flush_at > 0 && count_ >= flush_at);
  });
}

} // namespace internal
} // namespace gtmeasy
