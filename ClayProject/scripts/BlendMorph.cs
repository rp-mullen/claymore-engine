using ClaymoreEngine;
using System.Numerics;
using System;

public class BlendMorph : ScriptComponent
   {
   private float morphSpeed = 1.0f;
   private float morphValue = 0.0f;
   private bool increasing = true;

   public override void OnCreate()
      {
      Console.WriteLine("BlendMorph script created!");
      }

   public void SetMorph(float value)
      {
      self.GetComponent<UnifiedMorphComponent>().SetUnifiedMorph("Sex", value);
      }

   public override void OnUpdate(float dt)
      {

      }
   }