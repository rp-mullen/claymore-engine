using System;
using System.Numerics;


public static class Globals
   {
   public static Random random = new Random();
   
   public static float RandomRange(float min, float max)
      {
      return (float)(random.NextDouble() * (max - min) + min);
      }
   }