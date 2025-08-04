// GameScripts/ScriptComponent.cs

using ClaymoreEngine;

public abstract class ScriptComponent
{
   public int EntityID { get; internal set; }

   private Transform? _transform;
   public Transform transform => _transform ??= new Transform(EntityID);

   public virtual void OnCreate() { }
    public virtual void OnUpdate(float dt) { }

    public virtual void Bind(Entity entity)
    {
      EntityID = entity.EntityID; 
    }
}
