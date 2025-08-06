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

        public override bool Equals(object? obj) => obj is Entity e && e.EntityID == EntityID;
        public override int GetHashCode() => EntityID;
    }
   }