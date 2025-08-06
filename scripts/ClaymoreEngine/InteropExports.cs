using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    public static class InteropExports
    {
        private static readonly Dictionary<IntPtr, GCHandle> _liveHandles = new();


        // Cached callback pointer so native can invoke C# types
        private static RegisterScriptCallback? _nativeCallback;

        // Exposed for manual call in EngineEntry
        public static IntPtr NativeCallbackPtr => Marshal.GetFunctionPointerForDelegate(_nativeCallback!);

        // Called from native to create a new script instance by class name
        public static IntPtr Script_Create(IntPtr classNamePtr)
        {
            try
            {
                string className = Marshal.PtrToStringAnsi(classNamePtr)!;
                var instance = ScriptFactory.Create(className);

                var handle = GCHandle.Alloc(instance, GCHandleType.Normal);
                var ptr = GCHandle.ToIntPtr(handle);
                _liveHandles[ptr] = handle;
                return ptr;

                return (IntPtr)handle;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[C#] Script_Create failed: {ex}");
                return IntPtr.Zero;
            }
        }

        public static void Script_Destroy(IntPtr handle)
        {
            if (_liveHandles.TryGetValue(handle, out var gch))
            {
                gch.Free();
                _liveHandles.Remove(handle);
            }
        }


        // Called when entity is created in native
        public static void Script_OnCreate(IntPtr handle, int entityId)
        {
            var gch = GCHandle.FromIntPtr(handle);
            var script = (ScriptComponent)gch.Target!;
            script.Bind(new Entity(entityId));
            script.OnCreate();
         Console.WriteLine($"[C#] OnCreate thread: managed={Environment.CurrentManagedThreadId}, os={ThreadIds.OsTid()}");
      }

        // Called once per frame
        public static void Script_OnUpdate(IntPtr handle, float dt)
        {
            var gch = GCHandle.FromIntPtr(handle); ;
            var script = (ScriptComponent)gch.Target!;
            script.OnUpdate(dt);
        }

        // Called from native to register all known script types (callback passed from C++)
        public static void RegisterAllScripts(IntPtr fnPtr)
        {
            Console.WriteLine("[C#] RegisterAllScripts invoked");

            try
            {
                _nativeCallback = Marshal.GetDelegateForFunctionPointer<RegisterScriptCallback>(fnPtr);
            }
            catch (Exception e)
            {
                Console.WriteLine($"[C#] Failed to get delegate: {e.Message}");
                return;
            }

            if (_nativeCallback == null)
            {
                Console.WriteLine("[C#] Native callback is null!");
                return;
            }

            foreach (var type in ScriptDomain.AllScriptTypes) // Replace with your actual static list
            {
                if (type.FullName != null)
                {
                    Console.WriteLine($"[C#] Registering: {type.FullName}");
                    _nativeCallback(type.FullName);
                }
            }
        }
    }

    // Delegates used in native interop
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate IntPtr Script_CreateDelegate(IntPtr className);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void Script_OnCreateDelegate(IntPtr handle, int entityId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void Script_OnUpdateDelegate(IntPtr handle, float dt);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void RegisterScriptCallback(string className);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void RegisterAllScriptsDelegate(IntPtr nativeCallback);
}
