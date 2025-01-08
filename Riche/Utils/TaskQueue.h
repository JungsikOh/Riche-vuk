#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

class TaskQueue {
 public:
  // 작업(push)
  void Push(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(task));
  }

  // 작업(pop) - 성공하면 true, 실패하면 false
  // (queue가 비었으면 false)
  bool TryPop(std::function<void()>& outTask) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;

    outTask = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  // queue가 비었는지 확인
  bool Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

 private:
  std::queue<std::function<void()>> queue_;
  mutable std::mutex mutex_;  // mutable -> Empty()도 lock
};