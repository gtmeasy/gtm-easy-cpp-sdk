#pragma once

#include "storage.hpp"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace gtmeasy {
namespace internal {

class Queue {
 public:
  Queue(Storage* storage, int max_queue_size);

  bool enqueue(const std::string& kind, const nlohmann::json& body, std::string* error);
  int size();
  int size_unlocked() const { return count_; }
  int dropped() const { return dropped_; }
  void set_max_queue_size(int n) { max_queue_size_ = n > 0 ? n : 1000; }

  std::vector<QueueItem> snapshot();
  bool compact_keep(const std::vector<char>& keep_prefix, int prefix_len, std::string* error);

  void notify();
  bool wait_for_work(int interval_ms, int flush_at, const std::function<bool()>& extra_wake);

  template <typename Pred>
  bool wait_until(int timeout_ms, Pred pred) {
    std::unique_lock<std::mutex> lock(mu_);
    if (timeout_ms <= 0) {
      return pred();
    }
    return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), pred);
  }

  std::mutex& mutex() { return mu_; }
  std::condition_variable& cv() { return cv_; }

 private:
  Storage* storage_;
  int max_queue_size_;
  int dropped_ = 0;
  std::mutex mu_;
  std::condition_variable cv_;
  int count_ = 0;
};

} // namespace internal
} // namespace gtmeasy
