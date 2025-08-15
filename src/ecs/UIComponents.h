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

// Common UI anchoring presets used by panels and text
enum class UIAnchorPreset : int {
    TopLeft = 0,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight
};

struct PanelComponent {
    // Top-left anchored position in canvas pixels
    glm::vec2 Position = {0.0f, 0.0f};
    // Size in pixels
    glm::vec2 Size = {100.0f, 100.0f};
    // Additional scaling factor (applied after Size)
    glm::vec2 Scale = {1.0f, 1.0f};
    // Pivot inside the panel rect (0..1)
    glm::vec2 Pivot = {0.5f, 0.5f};
    // Rotation in degrees (around pivot)
    float Rotation = 0.0f;

    // Anchor-based placement (optional)
    bool AnchorEnabled = false;
    UIAnchorPreset Anchor = UIAnchorPreset::TopLeft;
    glm::vec2 AnchorOffset = {0.0f, 0.0f};

    // Visuals
    AssetReference Texture; // type should be texture (e.g. type = 2)
    glm::vec4 UVRect = {0.0f, 0.0f, 1.0f, 1.0f}; // {u0, v0, u1, v1}
    glm::vec4 TintColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float Opacity = 1.0f;

    // Fill mode
    enum class FillMode { Stretch = 0, Tile = 1, NineSlice = 2 };
    FillMode Mode = FillMode::Stretch;
    // For Tile mode: how many repeats over the panel area
    glm::vec2 TileRepeat = {1.0f, 1.0f};
    // For NineSlice: normalized margins in UV (left, top, right, bottom)
    glm::vec4 SliceUV = {0.1f, 0.1f, 0.1f, 0.1f};
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


