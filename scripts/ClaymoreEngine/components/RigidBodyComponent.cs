using System.Numerics;

namespace ClaymoreEngine
{
    public class RigidBodyComponent : ComponentBase
    {
        public float Mass
        {
            get => ComponentInterop.GetRigidBodyMass(entity.EntityID);
            set => ComponentInterop.SetRigidBodyMass(entity.EntityID, value);
        }

        public bool IsKinematic
        {
            get => ComponentInterop.GetRigidBodyIsKinematic(entity.EntityID);
            set => ComponentInterop.SetRigidBodyIsKinematic(entity.EntityID, value);
        }

        public Vector3 LinearVelocity
        {
            get
            {
                ComponentInterop.GetRigidBodyLinearVelocity(entity.EntityID, out float x, out float y, out float z);
                return new Vector3(x, y, z);
            }
            set => ComponentInterop.SetRigidBodyLinearVelocity(entity.EntityID, value.X, value.Y, value.Z);
        }

        public Vector3 AngularVelocity
        {
            get
            {
                ComponentInterop.GetRigidBodyAngularVelocity(entity.EntityID, out float x, out float y, out float z);
                return new Vector3(x, y, z);
            }
            set => ComponentInterop.SetRigidBodyAngularVelocity(entity.EntityID, value.X, value.Y, value.Z);
        }
    }
}
