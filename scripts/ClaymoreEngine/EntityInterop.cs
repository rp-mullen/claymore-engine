using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public unsafe delegate void EntityInteropInitDelegate(IntPtr* functionPointers, int count);


    public static class EntityInterop
    {
        
        // Position
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetEntityPositionFn(int entityID, out float x, out float y, out float z);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetEntityPositionFn(int entityID, float x, float y, float z);

        // Rotation, Scale, etc. can follow same pattern...

        // Entity Lookup
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int FindEntityByNameFn([MarshalAs(UnmanagedType.LPStr)] string name);

        // Public delegate instances to be set from native
        public static GetEntityPositionFn GetEntityPosition;
        public static SetEntityPositionFn SetEntityPosition;
        public static FindEntityByNameFn FindEntityByName;


        public static unsafe void InitializeInteropExport(IntPtr* ptrs, int count)
        {
            InitializeInterop(ptrs[0], ptrs[1], ptrs[2]);
        }


        public static void InitializeInterop(
            IntPtr getPos,
            IntPtr setPos,
            IntPtr findByName)
        {
            GetEntityPosition = Marshal.GetDelegateForFunctionPointer<GetEntityPositionFn>(getPos);
            SetEntityPosition = Marshal.GetDelegateForFunctionPointer<SetEntityPositionFn>(setPos);
            FindEntityByName = Marshal.GetDelegateForFunctionPointer<FindEntityByNameFn>(findByName);

            Console.WriteLine("[Managed] EntityInterop delegates initialized.");
        }

        public static void InitializeInterop(IntPtr[] functionPointers, int count)
        {
            if (count < 3)
            {
                Console.WriteLine("[EntityInterop] Invalid number of function pointers.");
                return;
            }

            InitializeInterop(
                functionPointers[0],
                functionPointers[1],
                functionPointers[2]
            );
        }

        // Example high-level API
        public static Vector3 GetPosition(int entityID)
        {
            GetEntityPosition(entityID, out float x, out float y, out float z);
            return new Vector3(x, y, z);
        }

        public static void SetPosition(int entityID, Vector3 position)
        {
            SetEntityPosition(entityID, position.X, position.Y, position.Z);
        }

        public static int FindByName(string name)
        {
            return FindEntityByName(name);
        }
    }
}
