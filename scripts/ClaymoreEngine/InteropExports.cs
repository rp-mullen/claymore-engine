using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Reflection;
using System.Numerics;

namespace ClaymoreEngine
{
    public static class InteropExports
    {
        private static readonly Dictionary<IntPtr, GCHandle> _liveHandles = new();

        // ------------------ Reflection interop ------------------
        private enum NativePropertyType
        {
            Int = 0,
            Float = 1,
            Bool = 2,
            String = 3,
            Vector3 = 4,
            Entity = 5
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RegisterPropDelegate([MarshalAs(UnmanagedType.LPStr)] string klass,
                                                  [MarshalAs(UnmanagedType.LPStr)] string field,
                                                  int propType,
                                                  IntPtr boxedDefault);

        // Native side passes pointer to this delegate so managed can invoke it without P/Invoke
        private static RegisterPropDelegate? _registerPropNative;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetFieldDelegate(IntPtr scriptHandle,
                                              [MarshalAs(UnmanagedType.LPStr)] string field,
                                              IntPtr boxedValue);

        private static NativePropertyType ToNative(Type t)
        {
            if (t == typeof(int)) return NativePropertyType.Int;
            if (t == typeof(float)) return NativePropertyType.Float;
            if (t == typeof(bool)) return NativePropertyType.Bool;
            if (t == typeof(string)) return NativePropertyType.String;
            if (t == typeof(System.Numerics.Vector3)) return NativePropertyType.Vector3;
            if (t == typeof(Entity)) return NativePropertyType.Entity;
            return NativePropertyType.Int;
        }

        // Native will grab pointer to this method
        public static void SetManagedField(IntPtr handle, string field, IntPtr boxed)
        {
            try
            {
                var gch = GCHandle.FromIntPtr(handle);
                var inst = gch.Target;
                if (inst == null) return;
                var fi = inst.GetType().GetField(field, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
                if (fi == null) return;

                Type ft = fi.FieldType;
                object? value = null;
                if (ft == typeof(int))
                {
                    value = Marshal.ReadInt32(boxed);
                }
                else if (ft == typeof(float))
                {
                    value = Marshal.PtrToStructure<float>(boxed);
                }
                else if (ft == typeof(bool))
                {
                    value = Marshal.ReadByte(boxed) != 0;
                }
                else if (ft == typeof(string))
                {
                    value = Marshal.PtrToStringAnsi(boxed);
                }
                else if (ft == typeof(System.Numerics.Vector3))
                {
                    value = Marshal.PtrToStructure<System.Numerics.Vector3>(boxed);
                }
                else if (ft == typeof(Entity))
                {
                    int id = Marshal.ReadInt32(boxed);
                    value = new Entity(id);
                }
                else
                {
                    // Unsupported type
                    Console.WriteLine($"[C#] Unsupported SetManagedField type: {ft}");
                }

                if (value != null)
                    fi.SetValue(inst, value);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[C#] SetManagedField error: {ex}");
            }
        }


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

        // Native passes pointer to struct containing function pointers
        [StructLayout(LayoutKind.Sequential)]
        private struct ScriptRegistrationInterop
        {
            public IntPtr RegisterScriptTypePtr;
            public IntPtr RegisterScriptPropertyPtr;
        }

        // Called from native to register all known script types (callback passed from C++)
        public static void RegisterAllScripts(IntPtr interopPtr)
        {
            Console.WriteLine("[C#] RegisterAllScripts invoked");

            var interop = Marshal.PtrToStructure<ScriptRegistrationInterop>(interopPtr);

            _nativeCallback = Marshal.GetDelegateForFunctionPointer<RegisterScriptCallback>(interop.RegisterScriptTypePtr);
            _registerPropNative = Marshal.GetDelegateForFunctionPointer<RegisterPropDelegate>(interop.RegisterScriptPropertyPtr);

            if (_nativeCallback == null || _registerPropNative == null)
            {
                Console.WriteLine("[C#] Native callbacks are null!");
                return;
            }

            // Register reflected fields before we notify native of script types
            foreach (var t in ScriptDomain.AllScriptTypes)
            {
                foreach (var field in t.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
                {
                    if (field.GetCustomAttribute<SerializeField>() == null) continue;

                    NativePropertyType nType = ToNative(field.FieldType);
                    object? defVal = field.FieldType.IsValueType ? Activator.CreateInstance(field.FieldType) : null;
                    IntPtr boxedPtr = IntPtr.Zero;
                    if(defVal != null)
                    {
                        var h = GCHandle.Alloc(defVal, GCHandleType.Pinned);
                        boxedPtr = GCHandle.ToIntPtr(h);
                    }
                    _registerPropNative!(t.FullName!, field.Name, (int)nType, boxedPtr);
                }
            }

            foreach (var type in ScriptDomain.AllScriptTypes)
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
