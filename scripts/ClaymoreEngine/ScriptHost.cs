using System.Runtime.InteropServices;
using System;

public static class ScriptHost
{
    [UnmanagedCallersOnly(EntryPoint = "CreateScriptInstance")]
    public static IntPtr CreateScriptInstance(IntPtr classNamePtr)
    {
        string className = Marshal.PtrToStringUTF8(classNamePtr)!;

        Type type = Type.GetType(className);
        if (type == null || !typeof(ScriptComponent).IsAssignableFrom(type))
            return IntPtr.Zero;

        var obj = (ScriptComponent)Activator.CreateInstance(type)!;
        GCHandle handle = GCHandle.Alloc(obj);
        return (IntPtr)handle;
    }
}