using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
   {
    public class Transform
    {
        private readonly int _entityID;

        internal Transform(int entityID)
        {
            _entityID = entityID;
        }

        public Vector3 position
        {
            get => EntityInterop.GetPosition(_entityID);
            set => EntityInterop.SetPosition(_entityID, value);
        }

        // You can expand later with rotation, scale, LookAt, etc.
        // public Quaternion rotation { get; set; }
        // public Vector3 scale { get; set; }
    }
}
