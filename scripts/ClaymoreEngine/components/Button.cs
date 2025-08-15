using System;

namespace ClaymoreEngine
{
    public sealed class Button : ComponentBase
    {
        public event Action OnClicked;
        public event Action OnPressed;
        public event Action OnReleased;
        public event Action OnHovered;
        public event Action OnUnhovered;

        private bool _prevPressed;
        private bool _prevHovered;

        internal void Update()
        {
            bool hovered = ComponentInterop.UI_ButtonIsHovered(entity.EntityID);
            bool pressed = ComponentInterop.UI_ButtonIsPressed(entity.EntityID);
            bool clicked = ComponentInterop.UI_ButtonWasClicked(entity.EntityID); // consumes click

            if (hovered && !_prevHovered) OnHovered?.Invoke();
            if (!hovered && _prevHovered) OnUnhovered?.Invoke();
            if (pressed && !_prevPressed) OnPressed?.Invoke();
            if (!pressed && _prevPressed) OnReleased?.Invoke();
            if (clicked) OnClicked?.Invoke();

            _prevHovered = hovered;
            _prevPressed = pressed;
        }
    }
}


