using System;
using System.Runtime.InteropServices;

namespace ClaymoreEngine
{
    // Thin managed wrapper over native Animator/AnimationPlayer component
    public sealed class Animator : ComponentBase
    {
        public AnimatorController GetController()
        {
            return new AnimatorController(entity.EntityID);
        }
    }

    public sealed class AnimatorController
    {
        private readonly int _entityId;
        internal AnimatorController(int entityId) { _entityId = entityId; }

        public void SetBool(string name, bool value) => ComponentInterop.Animator_SetBool(_entityId, name, value);
        public void SetInt(string name, int value) => ComponentInterop.Animator_SetInt(_entityId, name, value);
        public void SetFloat(string name, float value) => ComponentInterop.Animator_SetFloat(_entityId, name, value);
        public void SetTrigger(string name) => ComponentInterop.Animator_SetTrigger(_entityId, name);
        public void ResetTrigger(string name) => ComponentInterop.Animator_ResetTrigger(_entityId, name);

        public bool GetBool(string name) => ComponentInterop.Animator_GetBool(_entityId, name);
        public int GetInt(string name) => ComponentInterop.Animator_GetInt(_entityId, name);
        public float GetFloat(string name) => ComponentInterop.Animator_GetFloat(_entityId, name);
        public bool GetTrigger(string name) => ComponentInterop.Animator_GetTrigger(_entityId, name);
    }
}



