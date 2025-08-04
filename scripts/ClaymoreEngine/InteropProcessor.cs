// ClaymoreEngine/Scripting/Interop.cs
using System;
using System.Runtime.InteropServices;

// Interop.cs
namespace ClaymoreEngine
   {
   public static class InteropProcessor
      {
      private static RegisterScriptCallback? _registerCallback;

      public static void SetRegisterCallback(RegisterScriptCallback callback)
         {
         _registerCallback = callback;
         }

      public static int ReloadScripts(IntPtr wstrPath)
         {
         try
            {
            string path = Marshal.PtrToStringUni(wstrPath)!;
            ScriptDomain.LoadScripts(path, _registerCallback);
            return 0;
            }
         catch (Exception ex)
            {
            Console.WriteLine("[C#] ReloadScripts error: " + ex);
            return -1;
            }
         }
      }

   public delegate int ReloadScriptsDelegate(IntPtr wstrPath);
   }
