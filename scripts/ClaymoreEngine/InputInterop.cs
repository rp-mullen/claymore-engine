using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public unsafe delegate void InputInteropInitDelegate(IntPtr* functionPointers, int count);

    /// <summary>
    /// Managed side wrapper around native InputInterop functions.
    /// Mirrors the pattern used by <see cref="EntityInterop"/>.
    /// </summary>
    public static unsafe class InputInterop
    {
        // ---------------------------------------------------------------------------------
        // Delegate definitions (must match native signatures & calling convention order)
        // ---------------------------------------------------------------------------------
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int  IsKeyHeldFn(int key);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int  IsKeyDownFn(int key);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int  IsMouseDownFn(int button);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void GetMouseDeltaFn(out float dx, out float dy);
[UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void LogFn([MarshalAs(UnmanagedType.LPStr)] string message);


        // ------------------------------------------------------------------------------
        // Resolved function pointers
        // ------------------------------------------------------------------------------
        public static IsKeyHeldFn     IsKeyHeld;
        public static IsKeyDownFn     IsKeyDown;
        public static IsMouseDownFn   IsMouseDown;
        public static GetMouseDeltaFn GetMouseDelta;
public static LogFn            Log;

        // ------------------------------------------------------------------------------
        // Initialization from native side.  Native passes 3 pointers in correct order.
        // ------------------------------------------------------------------------------
        public static void InitializeInteropExport(IntPtr* ptrs, int count) => InitializeInterop(ptrs, count);

        public static unsafe void InitializeInterop(IntPtr* ptrs, int count)
        {
            if (count < 5)
            {
                Console.WriteLine($"[InputInterop] Expected >=3 function pointers, received {count}.");
                return;
            }

            int i = 0;
            IsKeyHeld   = Marshal.GetDelegateForFunctionPointer<IsKeyHeldFn>(ptrs[i++]);
            IsKeyDown   = Marshal.GetDelegateForFunctionPointer<IsKeyDownFn>(ptrs[i++]);
            IsMouseDown = Marshal.GetDelegateForFunctionPointer<IsMouseDownFn>(ptrs[i++]);
            GetMouseDelta = Marshal.GetDelegateForFunctionPointer<GetMouseDeltaFn>(ptrs[i++]);
Log          = Marshal.GetDelegateForFunctionPointer<LogFn>(ptrs[i++]);

            Console.WriteLine("[Managed] InputInterop delegates initialized.");
        }
    }

    // -------------------------------------------------------------------------------------
    // Convenience API exposed to user scripts
    // -------------------------------------------------------------------------------------
    public static class Input
    {
        /// <summary>
        /// Returns true the frame the key became pressed.  Uses instantaneous state check
        /// for now (no edge detection) – can be expanded on managed side if required.
        /// </summary>
        public static bool GetKeyDown(KeyCode key)  => InputInterop.IsKeyDown((int)key) != 0;
        public static bool GetKey(KeyCode key)      => InputInterop.IsKeyHeld((int)key) != 0;

        /// <summary>
        /// Returns true if specified mouse button currently pressed (0 = left, 1 = right ...).
        /// </summary>
        public static bool GetMouseDown(int button) => InputInterop.IsMouseDown(button) != 0;

        public static Vector2 GetMouseDelta()
        {
            InputInterop.GetMouseDelta(out float dx, out float dy);
            return new Vector2(dx, dy);
        }
    }

    // -------------------------------------------------------------------------------------
    // Simple Debug.Log wrapper
    // -------------------------------------------------------------------------------------
    public static class Debug
    {
        public static void Log(string message)
        {
            InputInterop.Log(message);
        }
    }

    // -------------------------------------------------------------------------------------
    // Minimal subset of GLFW key codes exposed to scripts.
    // Extend as required.
    // -------------------------------------------------------------------------------------
    public enum KeyCode
    {
        // Alphanumeric keys – same values as ASCII (GLFW matches these)
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,

        Space      = 32,
        LeftShift  = 340,
        LeftControl= 341,
        LeftAlt    = 342,
        RightShift = 344,
        RightControl = 345,
        RightAlt     = 346,

        Up    = 265,
        Down  = 264,
        Left  = 263,
        Right = 262,

        Escape = 256,
        Enter  = 257,
        Tab    = 258,
        Backspace = 259,
    }
}
