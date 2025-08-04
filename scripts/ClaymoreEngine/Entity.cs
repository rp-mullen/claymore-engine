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
            int id = EntityInterop.FindByName(name);
            return id >= 0 ? id : -1;
        }

        public override bool Equals(object obj) => obj is Entity e && e.EntityID == EntityID;
        public override int GetHashCode() => EntityID;
    }
   }