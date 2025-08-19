using System;
using System.Collections.Generic;
namespace Claymore.Modules.RPG;

public class VitalComponent
   {
   public string VitalName;
   public float CurrentValue;
   public float MaxValue;

   public Action OnChanged;
   public Action OnDepleted;

   public VitalComponent(string name, float maxValue)
      {
      VitalName = name;
      MaxValue = maxValue;
      CurrentValue = maxValue;
      }

   public void ModifyValue(float val)
      {
      CurrentValue += val;
      if (CurrentValue > MaxValue)
         {
         CurrentValue = MaxValue;
         }
      if (CurrentValue < 0)
         {
         CurrentValue = 0;
         OnDepleted?.Invoke();
         }
      OnChanged?.Invoke();
      }
   }

