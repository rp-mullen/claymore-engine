using ClaymoreEngine;

public class TestScript : ScriptComponent
{
    public override void OnCreate()
    {
        Log("TestScriptManaged created successfully!");
    }

    public override void OnUpdate(float deltaTime)
    {
    }

    private void Log(string message)
    {
    }
}