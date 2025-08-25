using System.Collections.Generic;
using System;

namespace ClaymoreEngine
   {
   public class UnifiedMorphComponent : ComponentBase
      {

      public void SetUnifiedMorph(string shapeName, float value)
         {
         var shapeCount = ComponentInterop.UnifiedMorph_GetCount(entity.EntityID);

         for (int i = 0; i < shapeCount; i++)
            {
            var name = ComponentInterop.UnifiedMorph_GetName(entity.EntityID, i);
            if (name == shapeName)
               {
               ComponentInterop.UnifiedMorph_SetWeight(entity.EntityID, i, value);
               return;
               }
            }
         Console.WriteLine("Couldn't find morph name: " + shapeName);
         }

      }
   }
