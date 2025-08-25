// New: ScriptRegistry.cs
using System;
using System.Collections.Generic;

namespace ClaymoreEngine;

public static class ScriptRegistry
   {
   private static readonly Dictionary<(int, Type), ScriptComponent> _byEntityType = new();

   public static void Register(ScriptComponent s) =>
       _byEntityType[(s.EntityID, s.GetType())] = s;

   public static T Get<T>(int entityId) where T : ScriptComponent =>
       _byEntityType.TryGetValue((entityId, typeof(T)), out var s) ? (T)s : null;

   }

   // New: EntityExtensions for scripts
   public static class ScriptLookupExtensions
      {
      public static T GetScript<T>(this Entity e) where T : ScriptComponent =>
          ScriptRegistry.Get<T>(e.EntityID);
      }
   