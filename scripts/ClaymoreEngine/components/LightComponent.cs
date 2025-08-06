using System.Numerics;

namespace ClaymoreEngine
{
    public enum LightType
    {
        Directional,
        Point
    }

    public class LightComponent : ComponentBase
    {
      public LightComponent()
        : this(LightType.Directional, new Vector3(1, 1, 1), 1.0f) { }

      public LightComponent(LightType type)
          : this(type, new Vector3(1, 1, 1), 1.0f) { }

      public LightComponent(LightType type, Vector3 color)
          : this(type, color, 1.0f) { }

      public LightComponent(LightType type, Vector3 color, float intensity)
         {
         Type = type;
         Color = color;
         Intensity = intensity;
         }

      public LightType Type
        {
            get => (LightType)ComponentInterop.GetLightType(entity.EntityID);
            set => ComponentInterop.SetLightType(entity.EntityID, (int)value);
        }

        public Vector3 Color
        {
            get
            {
                ComponentInterop.GetLightColor(entity.EntityID, out float r, out float g, out float b);
                return new Vector3(r, g, b);
            }
            set => ComponentInterop.SetLightColor(entity.EntityID, value.X, value.Y, value.Z);
        }

        public float Intensity
        {
            get => ComponentInterop.GetLightIntensity(entity.EntityID);
            set => ComponentInterop.SetLightIntensity(entity.EntityID, value);
        }
    }
}
