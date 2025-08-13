using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public unsafe delegate void EntityInteropInitDelegate(IntPtr* functionPointers, int count);

    // -----------------------------------------------------------------------------
    // Managed side wrapper around native EntityInterop functions.
    // The order of delegates MUST match the order of function pointers passed from
    // the native side (see DotNetHost.cpp::SetupEntityInterop).
    // -----------------------------------------------------------------------------
    public static unsafe class EntityInterop
    {
        // ---------------------- Core Transform ----------------------
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void GetEntityPositionFn(int entityID, out float x, out float y, out float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetEntityPositionFn(int entityID, float x, float y, float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int  FindEntityByNameFn([MarshalAs(UnmanagedType.LPStr)] string name);

        // Entity management
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int  CreateEntityFn([MarshalAs(UnmanagedType.LPStr)] string name);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void DestroyEntityFn(int entityID);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int  GetEntityByIDFn(int entityID);

        // Rotation / Scale
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void GetRotationFn(int entityID, out float x, out float y, out float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetRotationFn(int entityID, float x, float y, float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void GetRotationQuatFn(int entityID, out float x, out float y, out float z, out float w);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetRotationQuatFn(int entityID, float x, float y, float z, float w);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void GetScaleFn(int entityID, out float x, out float y, out float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetScaleFn(int entityID, float x, float y, float z);

        // Physics
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetLinearVelocityFn(int entityID, float x, float y, float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetAngularVelocityFn(int entityID, float x, float y, float z);

        // ---------------------- Delegate instances ----------------------
        public static GetEntityPositionFn  GetEntityPosition;
        public static SetEntityPositionFn  SetEntityPosition;
        public static FindEntityByNameFn   FindEntityByName;

        public static CreateEntityFn       CreateEntity;
        public static DestroyEntityFn      DestroyEntity;
        public static GetEntityByIDFn      GetEntityByID;

        public static GetRotationFn        GetEntityRotation;
        public static SetRotationFn        SetEntityRotation;
        public static GetRotationQuatFn    GetEntityRotationQuat;
        public static SetRotationQuatFn    SetEntityRotationQuat;
        public static GetScaleFn           GetEntityScale;
        public static SetScaleFn           SetEntityScale;

        public static SetLinearVelocityFn  SetLinearVelocity;
        public static SetAngularVelocityFn SetAngularVelocity;

        // -----------------------------------------------------------------
        // Initialization from native side.  The native code passes an array
        // of function pointers in the exact order defined above.
        // -----------------------------------------------------------------
        public static void InitializeInteropExport(IntPtr* ptrs, int count) => InitializeInterop(ptrs, count);

        public static unsafe void InitializeInterop(IntPtr* ptrs, int count)
        {
            if (count < 14) // Expect 14 core functions now (added quat rot get/set)
            {
                Console.WriteLine($"[EntityInterop] Expected >=14 function pointers, received {count}.");
                return;
            }

            int i = 0;
            GetEntityPosition  = Marshal.GetDelegateForFunctionPointer<GetEntityPositionFn> (ptrs[i++]);
            SetEntityPosition  = Marshal.GetDelegateForFunctionPointer<SetEntityPositionFn> (ptrs[i++]);
            FindEntityByName   = Marshal.GetDelegateForFunctionPointer<FindEntityByNameFn>  (ptrs[i++]);

            CreateEntity       = Marshal.GetDelegateForFunctionPointer<CreateEntityFn>      (ptrs[i++]);
            DestroyEntity      = Marshal.GetDelegateForFunctionPointer<DestroyEntityFn>     (ptrs[i++]);
            GetEntityByID      = Marshal.GetDelegateForFunctionPointer<GetEntityByIDFn>     (ptrs[i++]);

            GetEntityRotation  = Marshal.GetDelegateForFunctionPointer<GetRotationFn>       (ptrs[i++]);
            SetEntityRotation  = Marshal.GetDelegateForFunctionPointer<SetRotationFn>       (ptrs[i++]);
            GetEntityRotationQuat = Marshal.GetDelegateForFunctionPointer<GetRotationQuatFn> (ptrs[i++]);
            SetEntityRotationQuat = Marshal.GetDelegateForFunctionPointer<SetRotationQuatFn> (ptrs[i++]);
            GetEntityScale     = Marshal.GetDelegateForFunctionPointer<GetScaleFn>          (ptrs[i++]);
            SetEntityScale     = Marshal.GetDelegateForFunctionPointer<SetScaleFn>          (ptrs[i++]);

            SetLinearVelocity  = Marshal.GetDelegateForFunctionPointer<SetLinearVelocityFn> (ptrs[i++]);
            SetAngularVelocity = Marshal.GetDelegateForFunctionPointer<SetAngularVelocityFn>(ptrs[i++]);

            // Initialize ComponentInterop with the remaining pointers
            var componentInteropPtrs = (void**)(ptrs + i);
            var remainingCount = count - i;
            ComponentInterop.Initialize(componentInteropPtrs, remainingCount);

            Console.WriteLine("[Managed] EntityInterop delegates initialized.");
        }

        // ---------------------- Convenience Wrappers ----------------------
        public static Vector3 GetPosition(int entityID)
        {
            GetEntityPosition(entityID, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }

        public static void SetPosition(int entityID, Vector3 position) => SetEntityPosition(entityID, position.X, position.Y, position.Z);

        public static int FindByName(string name) => FindEntityByName(name);

        public static Vector3 GetRotation(int entityID)
        {
            GetEntityRotation(entityID, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }

        public static void SetRotation(int entityID, Vector3 rot) => SetEntityRotation(entityID, rot.X, rot.Y, rot.Z);

        public static Quaternion GetRotationQuat(int entityID)
        {
            GetEntityRotationQuat(entityID, out float x, out float y, out float z, out float w);
            return new Quaternion(x, y, z, w);
        }

        public static void SetRotationQuat(int entityID, Quaternion q)
            => SetEntityRotationQuat(entityID, q.X, q.Y, q.Z, q.W);

        public static Vector3 GetScale(int entityID)
        {
            GetEntityScale(entityID, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }
        public static void SetScale(int entityID, Vector3 scale) => SetEntityScale(entityID, scale.X, scale.Y, scale.Z);

    }
}