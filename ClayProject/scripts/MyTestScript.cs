using ClaymoreEngine;
using System.Numerics;
using System.Threading.Tasks;
using System;

public class MyTestScript : ScriptComponent
   {
   private bool moving = false;
   private float moveSpeed = 6.0f;

   // how quickly we rotate toward the target heading (higher = snappier)
   private float rotationSmooth = 10f;

   // smoothed animator parameter (not just 0 or 5)
   private float speedParam = 0f;
    private bool running = false;
   [SerializeField]
   public Entity refEntity;

   // Camera orbit settings
   [SerializeField]
   public Entity cameraEntity;

   private float orbitDistance = 5.0f;
   private float orbitMinPitch = -30.0f; // degrees
   private float orbitMaxPitch = 70.0f;  // degrees
   private float mouseSensitivity = 0.1f; // deg per pixel
   private float yawDegrees = 0.0f;   // camera yaw around player (degrees)
   private float pitchDegrees = 20.0f; // camera pitch (degrees)
   private Vector3 cameraTargetOffset = new Vector3(0f, 1.6f, 0f); // aim around head height

   private bool mouseCaptured = false;

   public override void OnCreate()
      {
      Console.WriteLine("Made it to the start of OnCreate!");
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
      // Toggle capture
      if (Input.GetKeyDown(KeyCode.E))
         {
         mouseCaptured = !mouseCaptured;
         if (mouseCaptured)
            {
            Input.SetMouseMode(Input.MouseMode.Captured);
            }
         else
            {
            Input.SetMouseMode(Input.MouseMode.Free);
            }
         }

      if (Input.GetKey(KeyCode.F))
        {
            running = true;

        }
      else
        {
            running = false;
        }

      // Update orbit angles from mouse when captured
      if (mouseCaptured)
        {
            var md = Input.GetMouseDelta();
            yawDegrees += md.X * mouseSensitivity;
            pitchDegrees -= md.Y * mouseSensitivity;
            if (pitchDegrees < orbitMinPitch) pitchDegrees = orbitMinPitch;
            if (pitchDegrees > orbitMaxPitch) pitchDegrees = orbitMaxPitch;
        }

      // --- Gather input into a single move direction ---
      Vector3 moveDir = Vector3.Zero;
      if (Input.GetKey(KeyCode.W)) moveDir += new Vector3(0f, 0f, -1f);
      if (Input.GetKey(KeyCode.S)) moveDir += new Vector3(0f, 0f, 1f);
      if (Input.GetKey(KeyCode.A)) moveDir += new Vector3(-1f, 0f, 0f);
      if (Input.GetKey(KeyCode.D)) moveDir += new Vector3(1f, 0f, 0f);

      moving = moveDir.LengthSquared() > 1e-6f;

      if (moving)
         {
         // Normalize so speed is consistent on diagonals
         moveDir = Vector3.Normalize(moveDir);

         // If captured, move relative to camera yaw (third-person style)
         Vector3 worldMove = moveDir;
         if (mouseCaptured)
            {
            float yawRad = MathF.PI / 180f * yawDegrees;
            float cosY = MathF.Cos(yawRad);
            float sinY = MathF.Sin(yawRad);
            // rotate XZ by yaw around Y axis
            float x = worldMove.X * cosY + worldMove.Z * -sinY;
            float z = worldMove.X * sinY + worldMove.Z *  cosY;
            worldMove = new Vector3(x, 0f, z);
            }

            float factor = 1.0f;
            if (running)
            {
                factor = 3f;
            }
         // Move
         transform.position += worldMove * moveSpeed * factor * dt;

         // --- Smoothly rotate around Y toward the move direction in world space ---
         float targetYaw = MathF.Atan2(worldMove.X, worldMove.Z); // +Z forward, +X right
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
         float targetSpeed = moving ? 0.5f : 0f;
            if (running && moving)
            {
                targetSpeed = 1f;
            }
         float x = 1f;
         // exponential smoothing (frame-rate independent)
         float lerpT = 1f - MathF.Exp(-6f * dt); // 8 = snappiness; try 6�12
         speedParam = float.Lerp(speedParam, targetSpeed, lerpT);

         animator.GetController().SetFloat("Speed", speedParam);
         }

      // Maintain orbit camera lock
      if (cameraEntity.EntityID != 0)
         {
         UpdateOrbitCamera(dt);
         }
      }



   // Optional: utility if you want to drive motion elsewhere
   public void MovePlayer(float dt, Vector3 dir)
      {
      transform.position += dir * moveSpeed * dt;
      }

   // ---------------- Camera helpers ----------------
   private void UpdateOrbitCamera(float dt)
      {
      // Player target position (e.g., head)
      Vector3 targetPos = transform.position + cameraTargetOffset;

      // Convert yaw/pitch to forward vector (from camera towards player)
      float yawRad = MathF.PI / 180f * yawDegrees;
      float pitchRad = MathF.PI / 180f * pitchDegrees;

      Vector3 forward = new Vector3(
         MathF.Sin(yawRad) * MathF.Cos(pitchRad),
         MathF.Sin(pitchRad),
         MathF.Cos(yawRad) * MathF.Cos(pitchRad)
      );

      // Camera should sit behind the target, so position = target - forward * distance
      Vector3 camPos = targetPos - forward * orbitDistance;

      // Apply to camera entity transform
      var camXform = cameraEntity.transform;
      camXform.position = camPos;

      // Build look rotation so camera looks at target
      Vector3 toTarget = Vector3.Normalize(targetPos - camPos);
      Quaternion camRot = LookRotation(toTarget, Vector3.UnitY);
      camXform.rotation = camRot;
      }

   private static Quaternion LookRotation(Vector3 forward, Vector3 up)
      {
      // Orthonormal basis
      Vector3 z = Vector3.Normalize(forward);
      Vector3 x = Vector3.Normalize(Vector3.Cross(up, z));
      Vector3 y = Vector3.Cross(z, x);

      var m = new Matrix4x4(
         x.X, x.Y, x.Z, 0f,
         y.X, y.Y, y.Z, 0f,
         z.X, z.Y, z.Z, 0f,
         0f,  0f,  0f,  1f
      );
      return Quaternion.CreateFromRotationMatrix(m);
      }
   }


