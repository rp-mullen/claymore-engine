// JobSystem.h
#pragma once
#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class JobSystem {
public:
   explicit JobSystem(size_t threads =
      std::max(1u, std::thread::hardware_concurrency()))
      {
      start(threads);
      }
   ~JobSystem() { stop(); }

   // Queue a job (callable with no args)
   void Enqueue(std::function<void()> job) {
         {
         std::lock_guard<std::mutex> lk(m_);
         queue_.push_back(std::move(job));
         }
         cv_.notify_one();
      }

private:
   void start(size_t n) {
      stop_.store(false);
      workers_.reserve(n);
      for (size_t i = 0; i < n; ++i) {
         workers_.emplace_back([this] {
            for (;;) {
               std::function<void()> job;
               {
               std::unique_lock<std::mutex> lk(m_);
               cv_.wait(lk, [this] { return stop_.load() || !queue_.empty(); });
               if (stop_.load() && queue_.empty()) return;
               job = std::move(queue_.front());
               queue_.pop_front();
               }
               job(); // run outside lock
               }
            });
         }
      }

   void stop() {
      stop_.store(true);
      cv_.notify_all();
      for (auto& t : workers_) if (t.joinable()) t.join();
      workers_.clear();
      }

   std::vector<std::thread> workers_;
   std::deque<std::function<void()>> queue_;
   std::mutex m_;
   std::condition_variable cv_;
   std::atomic<bool> stop_{ false };
   };
