using ClaymoreEngine;
using System.Numerics;
using System.Threading.Tasks;
using System;

public class MyTestScript : ScriptComponent
{
    public override void OnCreate()
    {
        _ = DoAsyncMethod();
    }


    private async Task DoAsyncMethod()
    {
        await Task.Delay(1000);
        Console.WriteLine("MyTestScript created successfully after 1 second!");
        await Task.Delay(5000);
        EntityInterop.CreateEntity("MyTestEntity");
    }

    public override void OnUpdate(float dt)
    {
        transform.position = transform.position + new Vector3(0, 0, 1f) * dt;
    }
}