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

        // Rotation / Scale
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void GetRotationFn(int entityID, out float x, out float y, out float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetRotationFn(int entityID, float x, float y, float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void GetScaleFn(int entityID, out float x, out float y, out float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetScaleFn(int entityID, float x, float y, float z);

        // Physics
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetLinearVelocityFn(int entityID, float x, float y, float z);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetAngularVelocityFn(int entityID, float x, float y, float z);

        // Lighting
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetLightColorFn(int entityID, float r, float g, float b);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetLightIntensityFn(int entityID, float intensity);

        // BlendShapes
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void SetBlendShapeWeightFn(int entityID, [MarshalAs(UnmanagedType.LPStr)] string shape, float weight);

        // ---------------------- Delegate instances ----------------------
        public static GetEntityPositionFn  GetEntityPosition;
        public static SetEntityPositionFn  SetEntityPosition;
        public static FindEntityByNameFn   FindEntityByName;

        public static CreateEntityFn       CreateEntity;
        public static DestroyEntityFn      DestroyEntity;

        public static GetRotationFn        GetEntityRotation;
        public static SetRotationFn        SetEntityRotation;
        public static GetScaleFn           GetEntityScale;
        public static SetScaleFn           SetEntityScale;

        public static SetLinearVelocityFn  SetLinearVelocity;
        public static SetAngularVelocityFn SetAngularVelocity;

        public static SetLightColorFn      SetLightColor;
        public static SetLightIntensityFn  SetLightIntensity;

        public static SetBlendShapeWeightFn SetBlendShapeWeight;

        // -----------------------------------------------------------------
        // Initialization from native side.  The native code passes an array
        // of function pointers in the exact order defined above.
        // -----------------------------------------------------------------
        public static void InitializeInteropExport(IntPtr* ptrs, int count) => InitializeInterop(ptrs, count);

        public static unsafe void InitializeInterop(IntPtr* ptrs, int count)
        {
            if (count < 14)
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

            GetEntityRotation  = Marshal.GetDelegateForFunctionPointer<GetRotationFn>       (ptrs[i++]);
            SetEntityRotation  = Marshal.GetDelegateForFunctionPointer<SetRotationFn>       (ptrs[i++]);
            GetEntityScale     = Marshal.GetDelegateForFunctionPointer<GetScaleFn>          (ptrs[i++]);
            SetEntityScale     = Marshal.GetDelegateForFunctionPointer<SetScaleFn>          (ptrs[i++]);

            SetLinearVelocity  = Marshal.GetDelegateForFunctionPointer<SetLinearVelocityFn> (ptrs[i++]);
            SetAngularVelocity = Marshal.GetDelegateForFunctionPointer<SetAngularVelocityFn>(ptrs[i++]);

            SetLightColor      = Marshal.GetDelegateForFunctionPointer<SetLightColorFn>     (ptrs[i++]);
            SetLightIntensity  = Marshal.GetDelegateForFunctionPointer<SetLightIntensityFn> (ptrs[i++]);

            SetBlendShapeWeight= Marshal.GetDelegateForFunctionPointer<SetBlendShapeWeightFn>(ptrs[i++]);

            Console.WriteLine("[Managed] EntityInterop delegates initialized (extended set).");
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

        public static Vector3 GetScale(int entityID)
        {
            GetEntityScale(entityID, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }
        public static void SetScale(int entityID, Vector3 scale) => SetEntityScale(entityID, scale.X, scale.Y, scale.Z);

    }
}