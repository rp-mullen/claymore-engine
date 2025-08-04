using ClaymoreEngine;
using System;

public static class ScriptFactory
{
   public static ScriptComponent Create(string className)
   {
      Type type = ScriptDomain.ResolveType(className);
      if (type == null)
      {
         Console.WriteLine("[C#] Available script types:");
         foreach (var t in ScriptDomain.AllScriptTypes)
            Console.WriteLine($"  - {t.FullName}");

         throw new Exception($"Script class '{className}' not found.");
      }

      return (ScriptComponent)Activator.CreateInstance(type);
   }
}