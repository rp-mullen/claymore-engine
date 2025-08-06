using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    // These delegates must match the C++ function pointer types in DotNetHost.h
    
    // Component Lifetime
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate bool HasComponentFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string componentName);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void AddComponentFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string componentName);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void RemoveComponentFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string componentName);

    // LightComponent
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int GetLightTypeFn(int entityId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void SetLightTypeFn(int entityId, int type);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void GetLightColorFn(int entityId, out float r, out float g, out float b);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void SetLightColorFn(int entityId, float r, float g, float b);
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate float GetLightIntensityFn(int entityId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void SetLightIntensityFn(int entityId, float intensity);

    // RigidBodyComponent
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate float GetRigidBodyMassFn(int entityId);
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void SetRigidBodyMassFn(int entityId, float mass);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate bool GetRigidBodyIsKinematicFn(int entityId);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void SetRigidBodyIsKinematicFn(int entityId, bool isKinematic);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void GetRigidBodyLinearVelocityFn(int entityId, out float x, out float y, out float z);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void SetRigidBodyLinearVelocityFn(int entityId, float x, float y, float z);
    
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void GetRigidBodyAngularVelocityFn(int entityId, out float x, out float y, out float z);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void SetRigidBodyAngularVelocityFn(int entityId, float x, float y, float z);

    // BlendShapeComponent
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void SetBlendShapeWeightFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string shapeName, float weight);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate float GetBlendShapeWeightFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string shapeName);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int GetBlendShapeCountFn(int entityId);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate IntPtr GetBlendShapeNameFn(int entityId, int index);


    internal static unsafe class ComponentInterop
    {
        // --- Function pointers ---
        public static HasComponentFn HasComponent;
        public static AddComponentFn AddComponent;
        public static RemoveComponentFn RemoveComponent;

        // Light
        public static GetLightTypeFn GetLightType;
        public static SetLightTypeFn SetLightType;
        public static GetLightColorFn GetLightColor;
        public static SetLightColorFn SetLightColor;
        public static GetLightIntensityFn GetLightIntensity;
        public static SetLightIntensityFn SetLightIntensity;

        // RigidBody
        public static GetRigidBodyMassFn GetRigidBodyMass;
        public static SetRigidBodyMassFn SetRigidBodyMass;
        public static GetRigidBodyIsKinematicFn GetRigidBodyIsKinematic;
        public static SetRigidBodyIsKinematicFn SetRigidBodyIsKinematic;
        public static GetRigidBodyLinearVelocityFn GetRigidBodyLinearVelocity;
        public static SetRigidBodyLinearVelocityFn SetRigidBodyLinearVelocity;
        public static GetRigidBodyAngularVelocityFn GetRigidBodyAngularVelocity;
        public static SetRigidBodyAngularVelocityFn SetRigidBodyAngularVelocity;

        // BlendShapes
        public static SetBlendShapeWeightFn SetBlendShapeWeight;
        public static GetBlendShapeWeightFn GetBlendShapeWeight;
        public static GetBlendShapeCountFn GetBlendShapeCount;
        public static GetBlendShapeNameFn GetBlendShapeNameInternal;

        public static string GetBlendShapeName(int entityId, int index)
        {
            IntPtr ptr = GetBlendShapeNameInternal(entityId, index);
            return Marshal.PtrToStringAnsi(ptr);
        }

        public static void Initialize(void** ptrs, int count)
        {
            if (count < 21) // Update this count as you add more functions
            {
                Console.WriteLine($"[ComponentInterop] Expected at least 21 function pointers, but got {count}.");
                return;
            }

            int i = 0;
            HasComponent = Marshal.GetDelegateForFunctionPointer<HasComponentFn>((IntPtr)ptrs[i++]);
            AddComponent = Marshal.GetDelegateForFunctionPointer<AddComponentFn>((IntPtr)ptrs[i++]);
            RemoveComponent = Marshal.GetDelegateForFunctionPointer<RemoveComponentFn>((IntPtr)ptrs[i++]);
            
            GetLightType = Marshal.GetDelegateForFunctionPointer<GetLightTypeFn>((IntPtr)ptrs[i++]);
            SetLightType = Marshal.GetDelegateForFunctionPointer<SetLightTypeFn>((IntPtr)ptrs[i++]);
            GetLightColor = Marshal.GetDelegateForFunctionPointer<GetLightColorFn>((IntPtr)ptrs[i++]);
            SetLightColor = Marshal.GetDelegateForFunctionPointer<SetLightColorFn>((IntPtr)ptrs[i++]);
            GetLightIntensity = Marshal.GetDelegateForFunctionPointer<GetLightIntensityFn>((IntPtr)ptrs[i++]);
            SetLightIntensity = Marshal.GetDelegateForFunctionPointer<SetLightIntensityFn>((IntPtr)ptrs[i++]);

            GetRigidBodyMass = Marshal.GetDelegateForFunctionPointer<GetRigidBodyMassFn>((IntPtr)ptrs[i++]);
            SetRigidBodyMass = Marshal.GetDelegateForFunctionPointer<SetRigidBodyMassFn>((IntPtr)ptrs[i++]);
            GetRigidBodyIsKinematic = Marshal.GetDelegateForFunctionPointer<GetRigidBodyIsKinematicFn>((IntPtr)ptrs[i++]);
            SetRigidBodyIsKinematic = Marshal.GetDelegateForFunctionPointer<SetRigidBodyIsKinematicFn>((IntPtr)ptrs[i++]);
            GetRigidBodyLinearVelocity = Marshal.GetDelegateForFunctionPointer<GetRigidBodyLinearVelocityFn>((IntPtr)ptrs[i++]);
            SetRigidBodyLinearVelocity = Marshal.GetDelegateForFunctionPointer<SetRigidBodyLinearVelocityFn>((IntPtr)ptrs[i++]);
            GetRigidBodyAngularVelocity = Marshal.GetDelegateForFunctionPointer<GetRigidBodyAngularVelocityFn>((IntPtr)ptrs[i++]);
            SetRigidBodyAngularVelocity = Marshal.GetDelegateForFunctionPointer<SetRigidBodyAngularVelocityFn>((IntPtr)ptrs[i++]);

            SetBlendShapeWeight = Marshal.GetDelegateForFunctionPointer<SetBlendShapeWeightFn>((IntPtr)ptrs[i++]);
            GetBlendShapeWeight = Marshal.GetDelegateForFunctionPointer<GetBlendShapeWeightFn>((IntPtr)ptrs[i++]);
            GetBlendShapeCount = Marshal.GetDelegateForFunctionPointer<GetBlendShapeCountFn>((IntPtr)ptrs[i++]);
            GetBlendShapeNameInternal = Marshal.GetDelegateForFunctionPointer<GetBlendShapeNameFn>((IntPtr)ptrs[i++]);
        }
    }
}

