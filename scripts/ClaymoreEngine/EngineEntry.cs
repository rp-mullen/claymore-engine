using System;
using System.IO;

namespace ClaymoreEngine
{
    public delegate int EntryPointDelegate(IntPtr args, int size);

    public static class EngineEntry
    {
        public static int ManagedStart(IntPtr args, int size)
    {
         Console.WriteLine("[C#] ManagedStart invoked!");


         string scriptsPath = Path.Combine(AppContext.BaseDirectory, "GameScripts.dll");
         if (File.Exists(scriptsPath))
         {
            ScriptDomain.LoadScripts(scriptsPath);
         }
         else
         {
            Console.WriteLine("[C#] GameScripts.dll not found at: " + scriptsPath);
         }

         return 0;
        }
    }
}
