using System.Collections.Generic;

namespace Claymore.Modules.RPG;


public class Stat
   {

   public string StatName;

   public float StatValue;
   }

public class Stats
   {

   public static List<Stat> StatDefinition;

   public Dictionary<string,float> stats { get; set; }

   public int StatCount => stats.Keys.Count;


   public static void DefineStat(Stat s)
      {
      StatDefinition.Add(s);
      }

   public static void RemoveDefinedStat(Stat stat)
      {
      foreach (var s in StatDefinition)
         {
         if ((s.StatName == stat.StatName))
            {
            StatDefinition.Remove(s);
            }
         }
      }


   public void ModifyValue(string key, int val)
      {
      if (stats.ContainsKey(key))
         {
         stats[key] += val;
         }
      else
         {
         stats.Add(key, val);
         }
      }

   public void SetValue(int idx, int val)
      {
      if (idx < 0 || idx >= StatCount)
         {
         return;
         }
      var key = stats.Keys.ElementAt(idx);
      if (stats.ContainsKey(key))
         {
         stats[key] = val;
         }
      else
         {
         stats.Add(key, val);
         }
      }

   public Stat GetStatByName(string statName)
      {
      if (stats.ContainsKey(statName))
         {
         return new Stat
            {
            StatName = statName,
            StatValue = stats[statName]
            };
         }

      return null;
      }

   }