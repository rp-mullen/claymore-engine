using ClaymoreEngine;
using System.Numerics;
using System.Threading.Tasks;
using System;

public class TestScriptManaged : ScriptComponent
{
    public override void OnCreate()
    {
        _ = DoAsyncMethod();
    }

    private async Task DoAsyncMethod()
    {
        Console.WriteLine("TestScriptManaged created successfully!");
        await Task.Delay(1000);
        Console.WriteLine("TestScriptManaged finished async initialization after 1 second!");
        await Task.Delay(5000);
        Entity newEntity = Entity.Create("MyTestEntity");

    }

    public override void OnUpdate(float dt)
    {
        transform.position += new Vector3(1f, 0f, 0f) * dt;
    }

    private void Log(string message)
    {
    }
}