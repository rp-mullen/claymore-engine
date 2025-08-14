#pragma once
#include "JobSystem.h"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <type_traits>
#include <algorithm>

// Use std::latch if you have C++20 <latch>; otherwise use a heap-owned cv/mutex.
#if defined(__cpp_lib_latch) && __cpp_lib_latch >= 201907L
#include <latch>
#define CM_USE_LATCH 1
#else
#define CM_USE_LATCH 0
#endif

template<class Fn>
inline void parallel_for(JobSystem& js,
   size_t begin, size_t end, size_t chunk,
   Fn&& fn)
   {
   if (end <= begin) return;

   const size_t total = end - begin;
   const size_t groups = (total + chunk - 1) / chunk;

   // Store the first exception from any slice.
   auto first_error = std::make_shared<std::exception_ptr>();
   std::mutex err_m;

   // Make the callable lifetime-safe for jobs (never capture &fn).
   using FnT = std::decay_t<Fn>;
   auto fn_holder = std::make_shared<FnT>(std::forward<Fn>(fn));

#if CM_USE_LATCH
   // Latch lives on the stack, but it is safe because we wait before returning.
   std::latch done((std::ptrdiff_t)groups);
   auto done_ptr = &done; // capture pointer
#else
   // Heap-owned sync objects so the last job can safely signal even if the caller
   // throws and unwinds past this function (belt-and-suspenders).
   struct Sync {
      std::mutex m;
      std::condition_variable cv;
      std::atomic<size_t> remaining{ 0 };
      };
   auto sync = std::make_shared<Sync>();
   sync->remaining.store(groups, std::memory_order_relaxed);
#endif

   size_t enqueued = 0;
   for (size_t s = begin; s < end; s += chunk) {
      const size_t c = std::min(chunk, end - s);

      bool ok = js.Enqueue([s, c, fn_holder, first_error, &err_m
#if CM_USE_LATCH
         , done_ptr
#else
         , sync
#endif
      ] {
         try {
            (*fn_holder)(s, c);
            }
         catch (...) {
            std::lock_guard<std::mutex> lk(err_m);
            if (!*first_error) *first_error = std::current_exception();
            }

#if CM_USE_LATCH
         done_ptr->count_down();
#else
         // Last-job wakeup with heap-owned sync to avoid dangling cv/mutex.
         if (sync->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lk(sync->m);
            sync->cv.notify_one();
            }
#endif
         });

      // If enqueue fails (shutdown), run slice inline to guarantee progress.
      if (!ok) {
         try {
            (*fn_holder)(s, c);
            }
         catch (...) {
            std::lock_guard<std::mutex> lk(err_m);
            if (!*first_error) *first_error = std::current_exception();
            }
#if CM_USE_LATCH
         done_ptr->count_down();
#else
         if (sync->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard<std::mutex> lk(sync->m);
            sync->cv.notify_one();
            }
#endif
         }
      else {
         ++enqueued;
         }
      }

#if CM_USE_LATCH
   done.wait();
#else
   // Wait until remaining == 0
   std::unique_lock<std::mutex> lk(sync->m);
   sync->cv.wait(lk, [&] { return sync->remaining.load(std::memory_order_acquire) == 0; });
#endif

   if (*first_error) std::rethrow_exception(*first_error);
   }
