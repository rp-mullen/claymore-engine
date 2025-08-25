namespace ClaymoreEngine
{
   public struct Entity
   {
      public int EntityID { get; private set; }

      public Entity(int entityID)
      {
         EntityID = entityID;
      }

      public Transform transform => new Transform(EntityID);

      public static int Find(string name)
      {
         return EntityInterop.FindByName(name);
      }

      public static Entity Create(string name)
      {
         int id = EntityInterop.CreateEntity(name);
         return new Entity(id);
      }

      public static Entity GetEntity(int id)
      {
         int entityId = EntityInterop.GetEntityByID(id);
         return new Entity(entityId);
      }

      public static Entity FindFirstEntityByType<T>()
      {
         var typeName = typeof(T).Name;
         int[] entityIds = EntityInterop.GetEntities();
         foreach (var entityId in entityIds)
         {
            if (ComponentInterop.HasComponent(entityId, typeName))
            {
               return new Entity(entityId);
            }
         }
         return new Entity(-1);
      }

      public static Entity FindFirstEntityByScript<T>() where T : ScriptComponent
      {
         var typeName = typeof(T).Name;
         int[] entityIds = EntityInterop.GetEntities();
         foreach (var entityId in entityIds)
         {
            if (new Entity(entityId).GetScript<T>() != null)
            {
               return new Entity(entityId);
            }
         }
         return new Entity(-1);
      }
      public override bool Equals(object? obj) => obj is Entity e && e.EntityID == EntityID;
      public override int GetHashCode() => EntityID;

      public static bool operator ==(Entity? left, Entity? right)
      {
         if (ReferenceEquals(left, right))
            return true;
         if (left is null || right is null)
            return false;

         return left?.EntityID == right?.EntityID;
      }

      public static bool operator !=(Entity? left, Entity? right) => !(left == right);

   }
}