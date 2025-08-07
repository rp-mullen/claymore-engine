using ClaymoreEngine;
using System.Numerics;
using System.Threading.Tasks;
using System;

public class MyTestScript : ScriptComponent
   {

   [SerializeField]
   public float Speed = 1.0f;

   public override void OnCreate()
      {
      Console.WriteLine("Made it to the start of OnCreate");
      _ = DoAsyncMethod();
      }


   private async Task DoAsyncMethod()
      {
      Console.WriteLine("[MyTestScript] Made it to the start of the async function.");
      await Task.Delay(1000);
      Console.WriteLine("MyTestScript created successfully after 1 second!");
      await Task.Delay(5000);
      Entity newEntity = Entity.Create("MyTestEntity");
      LightComponent l = newEntity.AddComponent<LightComponent>();
      l.Type = LightType.Point;
      l.Color = new Vector3(1, 0, 0);
      }

   public override void OnUpdate(float dt)
      {

      if (Input.GetKey(KeyCode.W))
         {
         MovePlayer(dt, new Vector3(0f, 0f, 1f));
         }


      if (Input.GetKey(KeyCode.A))
         {
         MovePlayer(dt, new Vector3(1f, 0f, 0f));
         }


      if (Input.GetKey(KeyCode.S))
         {
         MovePlayer(dt, new Vector3(0f, 0f, -1f));
         }


      if (Input.GetKey(KeyCode.D))
         {
         MovePlayer(dt, new Vector3(-1f, 0f, 0f));
         }
      }

   public void MovePlayer(float dt, Vector3 dir)
      {
      transform.position = transform.position + dir * dt * Speed;
      }
   }