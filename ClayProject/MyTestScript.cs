using ClaymoreEngine;
using System.Numerics;
using System.Threading.Tasks;
using System;

public class MyTestScript : ScriptComponent
   {
   private bool moving = false;
   private float moveSpeed = 3.0f;

   // how quickly we rotate toward the target heading (higher = snappier)
   private float rotationSmooth = 10f;

   // smoothed animator parameter (not just 0 or 5)
   private float speedParam = 0f;

   [SerializeField]
   public Entity refEntity;

   public override void OnCreate()
      {
      Console.WriteLine("Made it to the start of OnCreate");
      Console.WriteLine("[MyTestScript] Referencing Entity " + refEntity.EntityID);
      _ = DoAsyncMethod();
      }

   private async Task DoAsyncMethod()
      {
      Console.WriteLine("[MyTestScript] Made it to the start of the async function.");
      await Task.Delay(1000);
      Console.WriteLine("MyTestScript created successfully after 1 second!");
      await Task.Delay(5000);

      Entity newEntity = Entity.Create("MyTestEntity");
      Console.WriteLine("Created MyTestEntity");
      var l = newEntity.AddComponent<LightComponent>();
      l.Type = LightType.Point;
      l.Color = new Vector3(1, 0, 0);
      }

   public override void OnUpdate(float dt)
      {
      // --- Gather input into a single move direction ---
      Vector3 moveDir = Vector3.Zero;
      if (Input.GetKey(KeyCode.W)) moveDir += new Vector3(0f, 0f, 1f);
      if (Input.GetKey(KeyCode.S)) moveDir += new Vector3(0f, 0f, -1f);
      if (Input.GetKey(KeyCode.A)) moveDir += new Vector3(1f, 0f, 0f);
      if (Input.GetKey(KeyCode.D)) moveDir += new Vector3(-1f, 0f, 0f);

      moving = moveDir.LengthSquared() > 1e-6f;

      if (moving)
         {
         // Normalize so speed is consistent on diagonals
         moveDir = Vector3.Normalize(moveDir);

         // Move
         transform.position += moveDir * moveSpeed * dt;

         // --- Smoothly rotate around Y toward the move direction ---
         float targetYaw = MathF.Atan2(moveDir.X, moveDir.Z); // +Z forward, +X right
         Quaternion targetRot = Quaternion.CreateFromAxisAngle(Vector3.UnitY, targetYaw);

         // Critically-damped interpolation factor (frame-rate independent)
         float t = 1f - MathF.Exp(-rotationSmooth * dt); 
         transform.rotation = Quaternion.Slerp(transform.rotation, targetRot, t);
         }

      // --- Smooth Animator "Speed" param (no snapping) ---
      var self = new Entity(EntityID);
      var animator = self.GetComponent<Animator>();
      if (animator != null)
         {
         // scale to what your controller expects; using 0..5 like before
         float targetSpeed = moving ? 1f : 0f;
            float x = 1f;
         // exponential smoothing (frame-rate independent)
         float lerpT = 1f - MathF.Exp(-6f * dt); // 8 = snappiness; try 6–12
         speedParam = float.Lerp(speedParam, targetSpeed, lerpT);

         animator.GetController().SetFloat("Speed", speedParam);
         }
      }

   // Optional: utility if you want to drive motion elsewhere
   public void MovePlayer(float dt, Vector3 dir)
      {
      transform.position += dir * moveSpeed * dt;
      }
   }
