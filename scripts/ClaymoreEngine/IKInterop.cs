using System;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public unsafe delegate void IKInteropInitDelegate(IntPtr* functionPointers, int count);

    public static unsafe class IKInterop
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetWeightFn(int entity, float w);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetTargetFn(int entity, int targetEntity);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetPoleFn(int entity, int poleEntity);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetChainFn(int entity, int* boneIds, int count);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate float GetErrorMetersFn(int entity);

        public static SetWeightFn SetWeight;
        public static SetTargetFn SetTarget;
        public static SetPoleFn SetPole;
        public static SetChainFn SetChain;
        public static GetErrorMetersFn GetErrorMeters;

        public static void InitializeInteropExport(IntPtr* ptrs, int count)
        {
            if (count < 5) { Console.WriteLine($"[IKInterop] Expected 5 pointers, got {count}"); return; }
            int i = 0;
            SetWeight = Marshal.GetDelegateForFunctionPointer<SetWeightFn>(ptrs[i++]);
            SetTarget = Marshal.GetDelegateForFunctionPointer<SetTargetFn>(ptrs[i++]);
            SetPole   = Marshal.GetDelegateForFunctionPointer<SetPoleFn>(ptrs[i++]);
            SetChain  = Marshal.GetDelegateForFunctionPointer<SetChainFn>(ptrs[i++]);
            GetErrorMeters = Marshal.GetDelegateForFunctionPointer<GetErrorMetersFn>(ptrs[i++]);
            Console.WriteLine("[Managed] IKInterop delegates initialized.");
        }
    }
}


