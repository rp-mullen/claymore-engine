using System.Collections.Generic;

namespace ClaymoreEngine
{
    public class BlendShapeComponent : ComponentBase
    {
        public int ShapeCount => ComponentInterop.GetBlendShapeCount(entity.EntityID);

        public float GetWeight(string shapeName)
        {
            return ComponentInterop.GetBlendShapeWeight(entity.EntityID, shapeName);
        }

        public void SetWeight(string shapeName, float weight)
        {
            ComponentInterop.SetBlendShapeWeight(entity.EntityID, shapeName, weight);
        }

        public IEnumerable<string> GetShapeNames()
        {
            var count = ShapeCount;
            for (int i = 0; i < count; i++)
            {
                yield return ComponentInterop.GetBlendShapeName(entity.EntityID, i);
            }
        }
    }
}
