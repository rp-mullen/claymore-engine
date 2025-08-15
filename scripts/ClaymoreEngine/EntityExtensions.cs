using ClaymoreEngine;
using System;
using System.Collections.Generic;

namespace ClaymoreEngine
{
    public static class EntityExtensions
    {
        private static readonly Dictionary<(int, Type), object> _componentCache = new Dictionary<(int, Type), object>();

        public static T AddComponent<T>(this Entity entity) where T : ComponentBase, new()
        {
            var componentName = typeof(T).Name;
            if (ComponentInterop.HasComponent(entity.EntityID, componentName))
            {
                return GetComponent<T>(entity);
            }

            ComponentInterop.AddComponent(entity.EntityID, componentName);
            return GetComponent<T>(entity);
        }

        public static T GetComponent<T>(this Entity entity) where T : ComponentBase, new()
        {
            var key = (entity.EntityID, typeof(T));
            if (_componentCache.TryGetValue(key, out var cachedComponent))
            {
                return (T)cachedComponent;
            }

            var componentName = typeof(T).Name;
            if (!ComponentInterop.HasComponent(entity.EntityID, componentName))
            {
                return null;
            }

            var component = new T { entity = entity };
            _componentCache[key] = component;
            // If this is a UI Button, hook an updater so events fire each frame
            if (component is Button btn)
            {
                void Pump(object _)
                {
                    btn.Update();
                    System.Threading.SynchronizationContext.Current?.Post(Pump, null);
                }
                System.Threading.SynchronizationContext.Current?.Post(Pump, null);
            }
            return component;
        }

        public static void RemoveComponent<T>(this Entity entity) where T : ComponentBase
        {
            var key = (entity.EntityID, typeof(T));
            if (_componentCache.ContainsKey(key))
            {
                _componentCache.Remove(key);
            }
            
            var componentName = typeof(T).Name;
            ComponentInterop.RemoveComponent(entity.EntityID, componentName);
        }
    }
}
