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

    // Animator parameter setters
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void Animator_SetBoolFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name, bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void Animator_SetIntFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name, int value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void Animator_SetFloatFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name, float value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void Animator_SetTriggerFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate void Animator_ResetTriggerFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name);
    // Animator getters
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate bool  Animator_GetBoolFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate int   Animator_GetIntFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate float Animator_GetFloatFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] internal delegate bool  Animator_GetTriggerFn(int entityId, [MarshalAs(UnmanagedType.LPStr)] string name);


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

        // Animator
        public static Animator_SetBoolFn Animator_SetBool;
        public static Animator_SetIntFn Animator_SetInt;
        public static Animator_SetFloatFn Animator_SetFloat;
        public static Animator_SetTriggerFn Animator_SetTrigger;
        public static Animator_ResetTriggerFn Animator_ResetTrigger;
        public static Animator_GetBoolFn Animator_GetBool;
        public static Animator_GetIntFn Animator_GetInt;
        public static Animator_GetFloatFn Animator_GetFloat;
        public static Animator_GetTriggerFn Animator_GetTrigger;

        public static string GetBlendShapeName(int entityId, int index)
        {
            IntPtr ptr = GetBlendShapeNameInternal(entityId, index);
            return Marshal.PtrToStringAnsi(ptr);
        }

        public static void Initialize(void** ptrs, int count)
        {
            if (count < 30) // Update count for added animator functions
            {
                Console.WriteLine($"[ComponentInterop] Expected at least 30 function pointers, but got {count}.");
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

            Animator_SetBool = Marshal.GetDelegateForFunctionPointer<Animator_SetBoolFn>((IntPtr)ptrs[i++]);
            Animator_SetInt = Marshal.GetDelegateForFunctionPointer<Animator_SetIntFn>((IntPtr)ptrs[i++]);
            Animator_SetFloat = Marshal.GetDelegateForFunctionPointer<Animator_SetFloatFn>((IntPtr)ptrs[i++]);
            Animator_SetTrigger = Marshal.GetDelegateForFunctionPointer<Animator_SetTriggerFn>((IntPtr)ptrs[i++]);
            Animator_ResetTrigger = Marshal.GetDelegateForFunctionPointer<Animator_ResetTriggerFn>((IntPtr)ptrs[i++]);
            Animator_GetBool = Marshal.GetDelegateForFunctionPointer<Animator_GetBoolFn>((IntPtr)ptrs[i++]);
            Animator_GetInt = Marshal.GetDelegateForFunctionPointer<Animator_GetIntFn>((IntPtr)ptrs[i++]);
            Animator_GetFloat = Marshal.GetDelegateForFunctionPointer<Animator_GetFloatFn>((IntPtr)ptrs[i++]);
            Animator_GetTrigger = Marshal.GetDelegateForFunctionPointer<Animator_GetTriggerFn>((IntPtr)ptrs[i++]);
        }
    }
}

