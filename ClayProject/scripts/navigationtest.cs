using ClaymoreEngine;
using System.Numerics;
using System;

public class NavigationTest : ScriptComponent
   {
   private readonly Random rng = new Random();

   [SerializeField]
   public float roamRadius = 10f;

   [SerializeField]
   public float minWait = 0.5f;

   [SerializeField]
   public float maxWait = 2.0f;

   [SerializeField]
   public float arriveThreshold = 0.2f;

   private float nextMoveTimer = 0f;

   public override void OnCreate()
      {
      EnsureAgent();
      ScheduleNextMove(0.1f);
      }

   public override void OnUpdate(float dt)
      {
      if (!HasAgent()) return;

      nextMoveTimer -= dt;

      float remaining = float.MaxValue;
      if (NavigationInterop.AgentRemainingDistance != null)
         remaining = NavigationInterop.AgentRemainingDistance(EntityID);

      if (remaining <= arriveThreshold || nextMoveTimer <= 0f)
         {
         Vector3 pos = transform.position;
         Vector3 target = RandomPointAround(pos, roamRadius);
         var agent = self.GetComponent<NavAgentComponent>();
         agent?.SetDestination(target);
         ScheduleNextMove(RandomRange(minWait, maxWait));
         }
      }

   private void EnsureAgent()
      {
      if (self.GetComponent<NavAgentComponent>() == null)
         self.AddComponent<NavAgentComponent>();
      }

   private bool HasAgent()
      {
      return self.GetComponent<NavAgentComponent>() != null;
      }

   private Vector3 RandomPointAround(Vector3 center, float radius)
      {
      float angle = (float)(rng.NextDouble() * Math.PI * 2.0);
      float r = (float)rng.NextDouble() * radius;
      float x = center.X + (float)Math.Cos(angle) * r;
      float z = center.Z + (float)Math.Sin(angle) * r;
      return new Vector3(x, center.Y, z);
      }

   private float RandomRange(float a, float b)
      {
      return (float)(a + rng.NextDouble() * (b - a));
      }

   private void ScheduleNextMove(float t)
      {
      nextMoveTimer = t;
      }
   }


