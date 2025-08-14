// ParallelFor.h
#pragma once
#include "JobSystem.h"
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <algorithm>

template<class Fn>
inline void parallel_for(JobSystem& js,
   size_t begin, size_t end, size_t chunk,
   Fn&& fn)
   {
   if (end <= begin) return;
   const size_t total = end - begin;
   const size_t groups = (total + chunk - 1) / chunk;

   std::atomic<size_t> remaining{ groups };
   std::mutex m;
   std::condition_variable cv;

   for (size_t s = begin; s < end; s += chunk) {
      const size_t c = std::min(chunk, end - s);
      js.Enqueue([=, &fn, &remaining, &cv, &m] {
         fn(s, c);                        // your kernel over [s, s+c)
         if (remaining.fetch_sub(1) == 1) {
            std::lock_guard<std::mutex> lk(m);
            cv.notify_one();             // last job wakes waiter
            }
         });
      }

   std::unique_lock<std::mutex> lk(m);
   cv.wait(lk, [&] { return remaining.load() == 0; });
   }
