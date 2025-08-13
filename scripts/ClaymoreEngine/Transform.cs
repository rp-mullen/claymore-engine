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

      // Euler rotation in degrees
      public Vector3 eulerAngles
      {
         get => EntityInterop.GetRotation(_entityID);
         set => EntityInterop.SetRotation(_entityID, value);
      }

      // Quaternion rotation
      public Quaternion rotation
      {
         get => EntityInterop.GetRotationQuat(_entityID);
         set => EntityInterop.SetRotationQuat(_entityID, value);
      }

      public Vector3 scale
      {
         get => EntityInterop.GetScale(_entityID);
         set => EntityInterop.SetScale(_entityID, value);
      }

      // You can expand later with rotation, scale, LookAt, etc.
      // public Quaternion rotation { get; set; }
      // public Vector3 scale { get; set; }
   }
}
