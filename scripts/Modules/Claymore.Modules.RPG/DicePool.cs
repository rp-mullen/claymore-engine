using System;
using System.Collections.Generic;

namespace Claymore.Modules.RPG;

public enum Dice : uint
   {
   D2 = 2,
   D4 = 4,
   D6 = 6,
   D8 = 8,
   D10 = 10,
   D12 = 12,
   D20 = 20,
   D100 = 100
   }


public class DicePool
   {
   private Random _rng;

   public DicePool()
      {
      _rng = new Random();
      }

   public List<int> RollQuantity(Dice d, uint times)
      {
      List<int> rolls = new List<int>();
      for (int i = 0; i < times; i++)
         {
         rolls.Add(InternalRoll((uint)d));
         }
      return rolls;
      }

   public int RollWithModifier(Dice dice, uint modifier)
      {
      return InternalRoll((uint)dice) + (int)modifier;
      }

   public int Roll(Dice d)
      {
      return InternalRoll((uint)d);
      }

   private int InternalRoll(uint dice)
      {
      return 1 + _rng.Next((int)dice);
      }

   }