using ClaymoreEngine;
using System.Numerics;
using System.Threading.Tasks;
using System;

public class MyTestScript : ScriptComponent
{
    private bool moving = false;
    private float moveSpeed = 3.0f;

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
        moving = false;

        if (Input.GetKey(KeyCode.W)) { MovePlayer(dt, new Vector3(0f, 0f, 1f)); moving = true; }
        if (Input.GetKey(KeyCode.A)) { MovePlayer(dt, new Vector3(1f, 0f, 0f)); moving = true; }
        if (Input.GetKey(KeyCode.S)) { MovePlayer(dt, new Vector3(0f, 0f, -1f)); moving = true; }
        if (Input.GetKey(KeyCode.D)) { MovePlayer(dt, new Vector3(-1f, 0f, 0f)); moving = true; }

        var self = new Entity(EntityID);
        var animator = self.GetComponent<Animator>(); // NOTE: use Animator, not AnimatorComponent
        if (animator != null)
        {
            var ctrl = animator.GetController();
            ctrl.SetFloat("Speed", moving ? 5f : 0f);
        }
    }

    public void MovePlayer(float dt, Vector3 dir)
    {
        transform.position = transform.position + dir * dt * moveSpeed;
    }
}