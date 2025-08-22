using System.Numerics;

namespace ClaymoreEngine
{
    // Managed wrapper for native NavAgent component
    public sealed class NavAgentComponent : ComponentBase
    {
        public void SetDestination(Vector3 destination)
        {
            NavigationInterop.AgentSetDestination?.Invoke(entity.EntityID, destination);
        }

        public void Stop()
        {
            NavigationInterop.AgentStop?.Invoke(entity.EntityID);
        }

        public void Warp(Vector3 position)
        {
            NavigationInterop.AgentWarp?.Invoke(entity.EntityID, position);
        }

        public float RemainingDistance
        {
            get
            {
                return NavigationInterop.AgentRemainingDistance != null
                    ? NavigationInterop.AgentRemainingDistance(entity.EntityID)
                    : float.MaxValue;
            }
        }
    }
}


