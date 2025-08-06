using ClaymoreEngine;
using System.Numerics;
using System.Threading.Tasks;
using System;

public class MyTestScript : ScriptComponent
{
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
        transform.position = transform.position + new Vector3(0, 0, 1f) * dt;
    }
}