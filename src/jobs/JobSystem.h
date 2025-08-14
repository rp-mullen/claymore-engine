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
      start(threads ? threads : 1);
      }

   ~JobSystem() { stop(); }

   // Queue a job; returns false if system is stopping.
   bool Enqueue(std::function<void()> job) {
         {
         std::lock_guard<std::mutex> lk(m_);
         if (stopping_) return false;
         q_.push_back(std::move(job));
         }
         cv_.notify_one();
         return true;
      }

private:
   void start(size_t n) {
      stopping_ = false;
      workers_.reserve(n);
      for (size_t i = 0; i < n; ++i) {
         workers_.emplace_back([this] {
            for (;;) {
               std::function<void()> job;
               {
               std::unique_lock<std::mutex> lk(m_);
               cv_.wait(lk, [this] { return stopping_ || !q_.empty(); });
               if (stopping_ && q_.empty()) return;
               job = std::move(q_.front());
               q_.pop_front();
               }
               // Never let exceptions escape the worker thread.
               try { job(); }
               catch (...) {
#ifdef _MSC_VER
                  //OutputDebugStringA("[JobSystem] Unhandled job exception swallowed in worker.\n");
#endif
                  }
               }
            });
         }
      }

   void stop() {
         {
         std::lock_guard<std::mutex> lk(m_);
         stopping_ = true;
         }
         cv_.notify_all();
         for (auto& t : workers_) if (t.joinable()) t.join();
         workers_.clear();
         // Any leftover queued jobs are dropped on shutdown (by design).
      }

   std::vector<std::thread> workers_;
   std::deque<std::function<void()>> q_;
   std::mutex m_;
   std::condition_variable cv_;
   bool stopping_{ false };
   };
