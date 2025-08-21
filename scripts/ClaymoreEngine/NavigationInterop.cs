using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public unsafe delegate void NavigationInteropInitDelegate(IntPtr* functionPointers, int count);

    public static unsafe class NavigationInterop
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate bool FindPathFn(int navMeshEntity, Vector3 start, Vector3 end, NavAgentParams p, uint include, uint exclude, IntPtr outPath);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void AgentSetDestFn(int agentEntity, Vector3 dest);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void AgentStopFn(int agentEntity);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void AgentWarpFn(int agentEntity, Vector3 pos);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate float AgentRemainingFn(int agentEntity);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void OnPathCompleteFn(ulong managedHandle, bool success);

        public static FindPathFn FindPath;
        public static AgentSetDestFn AgentSetDestination;
        public static AgentStopFn AgentStop;
        public static AgentWarpFn AgentWarp;
        public static AgentRemainingFn AgentRemainingDistance;
        public static OnPathCompleteFn OnPathComplete;

        public static void InitializeInteropExport(IntPtr* ptrs, int count)
        {
            if (count < 6) { Console.WriteLine($"[NavigationInterop] Expected 6 pointers, got {count}"); return; }
            int i = 0;
            FindPath = Marshal.GetDelegateForFunctionPointer<FindPathFn>(ptrs[i++]);
            AgentSetDestination = Marshal.GetDelegateForFunctionPointer<AgentSetDestFn>(ptrs[i++]);
            AgentStop = Marshal.GetDelegateForFunctionPointer<AgentStopFn>(ptrs[i++]);
            AgentWarp = Marshal.GetDelegateForFunctionPointer<AgentWarpFn>(ptrs[i++]);
            AgentRemainingDistance = Marshal.GetDelegateForFunctionPointer<AgentRemainingFn>(ptrs[i++]);
            OnPathComplete = Marshal.GetDelegateForFunctionPointer<OnPathCompleteFn>(ptrs[i++]);
            Console.WriteLine("[Managed] NavigationInterop delegates initialized.");
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct NavAgentParams
        {
            public float radius, height, maxSlopeDeg, maxStep, maxSpeed, maxAccel;
        }
    }
}


