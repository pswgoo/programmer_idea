#pragma once
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace bt3 {

class ThreadPool {
public:
  explicit ThreadPool(std::size_t n_threads)
      : stop_(false) {
    if (n_threads == 0) n_threads = 1;
    workers_.reserve(n_threads);
    for (std::size_t i = 0; i < n_threads; ++i) {
      workers_.emplace_back([this]() { worker_loop(); });
    }
  }

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lk(mu_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
      if (t.joinable()) t.join();
    }
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  template <class F>
  auto submit(F&& f) -> std::future<decltype(f())> {
    using R = decltype(f());

    auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
    std::future<R> fut = task->get_future();

    {
      std::lock_guard<std::mutex> lk(mu_);
      tasks_.emplace([task]() { (*task)(); });
    }
    cv_.notify_one();
    return fut;
  }

private:
  void worker_loop() {
    for (;;) {
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [&]() { return stop_ || !tasks_.empty(); });
        if (stop_ && tasks_.empty()) return;
        job = std::move(tasks_.front());
        tasks_.pop();
      }
      job();
    }
  }

private:
  std::mutex mu_;
  std::condition_variable cv_;
  bool stop_;
  std::queue<std::function<void()>> tasks_;
  std::vector<std::thread> workers_;
};

} // namespace bt3
