#pragma once
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace bt3 {

class WorkerPool {
public:
  explicit WorkerPool(std::size_t n_workers)
      : n_(n_workers ? n_workers : 1),
        jobs_(n_),
        has_job_(n_, false),
        stop_(false) {
    threads_.reserve(n_);
    for (std::size_t i = 0; i < n_; ++i) {
      threads_.emplace_back([this, i]() { loop(i); });
    }
  }

  ~WorkerPool() {
    {
      std::lock_guard<std::mutex> lk(mu_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : threads_) if (t.joinable()) t.join();
  }

  std::size_t size() const { return n_; }

  // 同步执行：对每个 worker 分发一个 job，然后等待全部完成（barrier）
  void run_all(const std::vector<std::function<void(std::size_t)>>& per_worker_job) {
    // per_worker_job.size() 必须 == n_
    {
      std::lock_guard<std::mutex> lk(mu_);
      for (std::size_t i = 0; i < n_; ++i) {
        jobs_[i] = per_worker_job[i];
        has_job_[i] = true;
      }
      done_ = 0;
    }
    cv_.notify_all();

    // wait barrier
    std::unique_lock<std::mutex> lk(mu_);
    cv_done_.wait(lk, [&]() { return done_ == n_; });
  }

private:
  void loop(std::size_t wid) {
    for (;;) {
      std::function<void(std::size_t)> job;
      {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [&]() { return stop_ || has_job_[wid]; });
        if (stop_) return;
        job = std::move(jobs_[wid]);
        has_job_[wid] = false;
      }

      if (job) job(wid);

      {
        std::lock_guard<std::mutex> lk(mu_);
        ++done_;
        if (done_ == n_) cv_done_.notify_one();
      }
    }
  }

private:
  std::size_t n_;
  std::vector<std::thread> threads_;

  std::mutex mu_;
  std::condition_variable cv_;
  std::condition_variable cv_done_;
  bool stop_;
  std::vector<std::function<void(std::size_t)>> jobs_;
  std::vector<bool> has_job_;
  std::size_t done_{0};
};

} // namespace bt3