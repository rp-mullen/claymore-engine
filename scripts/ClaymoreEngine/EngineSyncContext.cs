using System;
using System.Collections.Concurrent;
using System.Threading;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    /// <summary>
    /// Simple main-thread SynchronizationContext. Continuations posted via
    /// await (e.g. await Task.Delay) will be queued here and executed once
    /// per frame from native side.
    /// </summary>
    public sealed class EngineSyncContext : SynchronizationContext
    {
        private readonly ConcurrentQueue<Action> _queue = new();

        // Singleton instance – created once in EngineEntry.ManagedStart
        public static EngineSyncContext Instance { get; } = new();

        private EngineSyncContext() { }

        public override void Post(SendOrPostCallback d, object? state)
        {
            _queue.Enqueue(() => d(state));
        }

        public override void Send(SendOrPostCallback d, object? state)
        {
            // We are already on the main thread – execute immediately.
            d(state);
        }

        /// <summary>
        /// Execute all queued actions. Must be called from the engine main thread
        /// after scripts have had their OnUpdate invoked.
        /// </summary>
        public void ExecutePending()
        {
            while (_queue.TryDequeue(out var action))
            {
                try { action(); }
                catch (Exception ex) { Console.WriteLine($"[EngineSyncContext] Exception: {ex}"); }
            }
        }

        // ---------------- Native hook ----------------
        // Expose a static method for native side to call once per frame.
        public static void Flush()
        {
            Instance.ExecutePending();
        }
    }

    // Delegate signature used by native side when loading function pointer
    public delegate void FlushDelegate();
}
