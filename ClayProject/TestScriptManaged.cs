using ClaymoreEngine;
using System.Numerics;
using System;

public class TestScriptManaged : ScriptComponent
{
    public override void OnCreate()
    {
        Console.WriteLine("TestScriptManaged created successfully!");
    }

    public override void OnUpdate(float dt)
    {
        transform.position += new Vector3(1f, 0f, 0f) * dt;
    }

    private void Log(string message)
    {
    }
}