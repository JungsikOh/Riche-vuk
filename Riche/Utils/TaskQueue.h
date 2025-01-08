#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

class TaskQueue {
 public:
  // �۾�(push)
  void Push(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(task));
  }

  // �۾�(pop) - �����ϸ� true, �����ϸ� false
  // (queue�� ������� false)
  bool TryPop(std::function<void()>& outTask) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;

    outTask = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  // queue�� ������� Ȯ��
  bool Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

 private:
  std::queue<std::function<void()>> queue_;
  mutable std::mutex mutex_;  // mutable -> Empty()�� lock
};