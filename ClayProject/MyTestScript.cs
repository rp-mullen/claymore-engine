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
         transform.position = transform.position + moveDir * dt * moveSpeed;

         // --- Smoothly rotate around Y toward the move direction ---
         // Convert movement direction to a yaw angle (radians)
         // Note: Atan2(x, z) gives yaw with +Z forward, +X to the right.
         float targetYaw = MathF.Atan2(moveDir.X, moveDir.Z);

         // Build a target orientation that only rotates around Y
         Quaternion targetRot = Quaternion.CreateFromAxisAngle(Vector3.UnitY, targetYaw);

         // Critically-damped interpolation factor (frame-rate independent)
         float t = 1f - MathF.Exp(-rotationSmooth * dt);

         // Slerp from current to target
         Quaternion current = transform.rotation;
         transform.rotation = Quaternion.Slerp(current, targetRot, t);
         }

      // Animator param based on movement
      var self = new Entity(EntityID);
      var animator = self.GetComponent<Animator>();
      if (animator != null)
         {
         var ctrl = animator.GetController();
         // Scale however your controller expects; using 0/5 like before:
         ctrl.SetFloat("Speed", moving ? 5f : 0f);
         }
      }

   // Kept for clarity; movement now handled in OnUpdate to compute heading
   public void MovePlayer(float dt, Vector3 dir)
      {
      transform.position = transform.position + dir * dt * moveSpeed;
      }
   }
