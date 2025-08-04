using ClaymoreEngine;
using System.Numerics;

public class MyTestScript : ScriptComponent
{
    public override void OnCreate()
    {
       
    }

    public override void OnUpdate(float dt)
    {
        transform.position = transform.position + new Vector3(0, 0, 1f) * dt;
    }
}