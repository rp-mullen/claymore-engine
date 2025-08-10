#pragma once

#include <glm/glm.hpp>
#include <pipeline/AssetReference.h>

// Runtime UI components: Canvas, Panel, Button

struct CanvasComponent {
    // If zero, canvas size is derived from framebuffer/window size
    int Width = 0;
    int Height = 0;

    // Global UI scale factor for DPI or user preference
    float DPIScale = 1.0f;

    enum class RenderSpace {
        ScreenSpace,
        WorldSpace
    };
    RenderSpace Space = RenderSpace::ScreenSpace;

    // Sorting order relative to other canvases (lower renders first)
    int SortOrder = 0;

    // If true, UI interactions on this canvas can block scene input
    bool BlockSceneInput = true;
};

struct PanelComponent {
    // Top-left anchored position in canvas pixels
    glm::vec2 Position = {0.0f, 0.0f};
    // Size in pixels
    glm::vec2 Size = {100.0f, 100.0f};
    // Pivot inside the panel rect (0..1)
    glm::vec2 Pivot = {0.5f, 0.5f};
    // Rotation in degrees (around pivot)
    float Rotation = 0.0f;

    // Visuals
    AssetReference Texture; // type should be texture (e.g. type = 2)
    glm::vec4 UVRect = {0.0f, 0.0f, 1.0f, 1.0f}; // {u0, v0, u1, v1}
    glm::vec4 TintColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float Opacity = 1.0f;
    bool Visible = true;
    int ZOrder = 0; // sorting within a canvas (lower renders first)
};

struct ButtonComponent {
    // Interaction state (runtime)
    bool Interactable = true;
    bool Hovered = false;
    bool Pressed = false;
    bool Clicked = false; // true for one frame when released after press

    // Toggle button behavior
    bool Toggle = false;
    bool Toggled = false;

    // Visual overrides by state (multiplied with panel tint)
    glm::vec4 NormalTint = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 HoverTint = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 PressedTint = {0.9f, 0.9f, 0.9f, 1.0f};

    // Optional feedback
    AssetReference HoverSound;  // e.g. type for audio asset
    AssetReference ClickSound;  // e.g. type for audio asset
};


