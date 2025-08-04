using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;

namespace ClaymoreEngine
   {
   public static class ScriptDomain
      {
      private static AssemblyLoadContext? _alc;
      private static Assembly? _scriptsAsm;

      public static readonly List<Type> _allScriptTypes = new();
      public static IEnumerable<Type> AllScriptTypes => _allScriptTypes;

      /// <summary>
      /// Loads the specified GameScripts DLL into a collectible context and registers all valid ScriptComponent types.
      /// </summary>
      public static void LoadScripts(string dllPath, RegisterScriptCallback? registerCallback = null)
         {
         if (!File.Exists(dllPath))
            throw new FileNotFoundException("GameScripts.dll not found", dllPath);

         // Unload old domain if present
         UnloadDomain();

         string scriptsDir = Path.GetDirectoryName(dllPath)!;
         _alc = new AssemblyLoadContext("GameScriptsDomain", isCollectible: true);

         // Resolve missing dependencies from the GameScripts folder
         _alc.Resolving += (context, asmName) =>
         {
            string candidate = Path.Combine(scriptsDir, $"{asmName.Name}.dll");
            if (File.Exists(candidate))
               {
               Console.WriteLine($"[C#] Resolving: {asmName} => {candidate}");
               return context.LoadFromAssemblyPath(candidate);
               }

            Console.WriteLine($"[C#] Failed to resolve: {asmName}");
            return null;
         };

         // Make sure ClaymoreEngine can be reused from default load context
         AssemblyLoadContext.Default.Resolving += ResolveFromDefault;

         try
            {
            string fullPath = Path.GetFullPath(dllPath);
            byte[] dllBytes = File.ReadAllBytes(fullPath);
            _scriptsAsm = _alc.LoadFromStream(new MemoryStream(dllBytes));


            Console.WriteLine($"[C#] Loaded scripts: {_scriptsAsm.FullName}");

            _allScriptTypes.Clear();

            foreach (Type type in _scriptsAsm.GetTypes())
               {
               if (type.IsAbstract || !typeof(ScriptComponent).IsAssignableFrom(type))
                  continue;

               Console.WriteLine($"[C#] Registering: {type.FullName}");
               _allScriptTypes.Add(type);

               if (registerCallback != null)
                  registerCallback(type.FullName!);
               }

            Console.WriteLine($"[C#] Total script types loaded: {_allScriptTypes.Count}");
            }
         catch (ReflectionTypeLoadException rtle)
            {
            Console.WriteLine($"[C#] ReflectionTypeLoadException: {rtle.Message}");
            foreach (var ex in rtle.LoaderExceptions)
               {
               Console.WriteLine($"  [LoaderException] {ex?.Message}");
               }

            throw;
            }
         catch (Exception ex)
            {
            Console.WriteLine($"[C#] Failed to load GameScripts: {ex}");
            throw;
            }
         }

      /// <summary>
      /// Attempt to resolve shared assemblies from default context (e.g., ClaymoreEngine).
      /// </summary>
      private static Assembly? ResolveFromDefault(AssemblyLoadContext context, AssemblyName assemblyName)
         {
         if (assemblyName.Name == "ClaymoreEngine")
            {
            var loaded = AppDomain.CurrentDomain.GetAssemblies()
                .FirstOrDefault(a => a.GetName().Name == "ClaymoreEngine");

            if (loaded != null)
               {
               Console.WriteLine($"[C#] Resolved ClaymoreEngine from default context");
               return loaded;
               }

            Console.WriteLine($"[C#] Failed to resolve ClaymoreEngine from default context");
            }

         return null;
         }

      /// <summary>
      /// Clean up previously loaded context and assemblies.
      /// </summary>
      private static void UnloadDomain()
         {
         if (_alc != null)
            {
            Console.WriteLine("[C#] Unloading previous script domain...");
            _scriptsAsm = null;

            _alc.Unload();
            _alc = null;

            GC.Collect();
            GC.WaitForPendingFinalizers();
            Console.WriteLine("[C#] Domain unloaded.");
            }
         }

      /// <summary>
      /// Attempts to resolve a script type by full name.
      /// </summary>
      public static Type? ResolveType(string name)
         {
         // Try exact full name first
         var type = _scriptsAsm?.GetType(name, throwOnError: false, ignoreCase: false);
         if (type != null)
            return type;

         // Fallback: try to find by short name
         return _allScriptTypes.FirstOrDefault(t => t.Name == name);
         }
      }
      }
