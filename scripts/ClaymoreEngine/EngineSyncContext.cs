using System;
using System.Collections.Concurrent;
using System.Threading;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
   {
   static class ThreadIds
      {
      [DllImport("kernel32.dll")]
      private static extern uint GetCurrentThreadId();
      public static uint OsTid() => GetCurrentThreadId();
      }

   /// <summary>
   /// Main-thread SynchronizationContext for running async continuations
   /// on the engine’s game loop thread.
   /// </summary>
   public sealed class EngineSyncContext : SynchronizationContext
      {
      private readonly ConcurrentQueue<Action> _queue = new();
      private int _mainThreadId = -1;

      // Singleton instance
      public static EngineSyncContext Instance { get; } = new();

      private EngineSyncContext() { }

      public void MarkInstalledHere()
         {
         _mainThreadId = Environment.CurrentManagedThreadId;
         }

      public override void Post(SendOrPostCallback d, object? state)
         {
         _queue.Enqueue(() => d(state));
         }

      public override void Send(SendOrPostCallback d, object? state)
         {
         if (Environment.CurrentManagedThreadId == _mainThreadId)
            d(state);
         else
            Post(d, state);
         }

      public void ExecutePending()
         {
         while (_queue.TryDequeue(out var action))
            {
            try { action(); }
            catch (Exception ex) { Console.WriteLine($"[EngineSyncContext] Exception: {ex}"); }
            }
         }

         // Clear any pending work without executing it. Useful when switching
         // between editor and play runtime to avoid executing stale continuations
         // captured under a previous scene/context.
         public static void Clear()
            {
            while (Instance._queue.TryDequeue(out _))
               {
               // intentionally empty
               }
            }

      // ---------------- Native hooks ----------------
      public static void Flush()
         {
         Instance.ExecutePending();
         }

      public static void InstallFromNative()
         {
         SynchronizationContext.SetSynchronizationContext(Instance);
         Instance.MarkInstalledHere();
         Console.WriteLine($"[C#] InstallFromNative: managed={Environment.CurrentManagedThreadId}, os={ThreadIds.OsTid()}");
         }

      public static void EnsureInstalledHereFromNative()
         {
         if (SynchronizationContext.Current != Instance)
            {
            SynchronizationContext.SetSynchronizationContext(Instance);
            Instance.MarkInstalledHere();
            Console.WriteLine($"[C#] EnsureInstalledHereFromNative: managed={Environment.CurrentManagedThreadId}, os={ThreadIds.OsTid()}");
            }
         }

      // Delegate signature for all native-callable void() methods

      }

   [UnmanagedFunctionPointer(CallingConvention.StdCall)]
   public delegate void VoidDelegate();
   }
